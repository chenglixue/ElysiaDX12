#include "stdafx.h"
#include "Material.h"

#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12BufferResource.h"
#include "lib/Utility/PipelineResourceUtility.h"
#include "RenderResource.h"
#include "Manager/ShaderVariantManager.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;
	
	// ---------- EqualBufferData ����������POD ��ȷ�Ƚϻ򸡵���ݲ ----------
	inline bool EqualBufferDataExact(const void* a, const void* b, size_t bytes) noexcept
	{
		return memcmp(a, b, bytes) == 0;
	}

	// �����ݲ�Ƚϣ����� Matrix/Vector��
	inline bool EqualFloatBufferWithTolerance(const float* a, const float* b, size_t count, float eps = 1e-6f) noexcept
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (fabsf(a[i] - b[i]) > eps) return false;
		}
		return true;
	}
	
	template<typename T>
	inline bool EqualBufferData(const T* a, const T* b, size_t count) noexcept
	{
		if constexpr (std::is_floating_point_v<T>)
		{
			return EqualFloatBufferWithTolerance(reinterpret_cast<const float*>(a), reinterpret_cast<const float*>(b), count);
		}
		else
		{
			return memcmp(a, b, sizeof(T) * count) == 0;
		}
	}
	
	Material::Material(DX12Device* pDevice, std::vector<ShaderPass>& shaderPasses) :
		m_pDevice(pDevice)
	{
		assert(pDevice);
		Init(shaderPasses);
	}

	Material::Material(DX12Device* pDevice, std::vector<ShaderPass>& shaderPasses, MeshRender* pMeshRender) :
		m_pDevice(pDevice),
		m_pMeshRender(pMeshRender)
	{
		assert(pDevice);
		Init(shaderPasses);
	}

	void Material::Init(std::vector<ShaderPass>& shaderPasses)
	{
		m_passDatas.reserve(shaderPasses.size());

		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			auto newPassData = PassData();

			// set shader pass
			newPassData.PassIndex = passID;
			newPassData.Name = shaderPasses[passID].Name;

			ShaderCreateDesc desc;
			if (!shaderPasses[passID].IsComputeShader)
			{
				desc.stages = 
				{
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Vertex,
						.ShaderName = shaderPasses[passID].FilePath,
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
					},
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Pixel,
						.ShaderName = shaderPasses[passID].FilePath,
						.EntryPoint = shaderPasses[passID].FragmentEntryPoint,
					}
				};
			}
			else
			{
				desc.stages = 
				{
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Compute,
						.EntryPoint = shaderPasses[passID].ComputeEntryPoint,
					}
				};
			}

			newPassData.pShader = std::move(m_pDevice->CreateShader(desc));
			newPassData.BlendDesc = GetBlendState(newPassData.pShader->GetRenderStates().at(L"Blend"));
			newPassData.RasterizerDesc = GetRasterizerState(newPassData.pShader->GetRenderStates().at(L"Rasterizer"));
			newPassData.DepthStencilDesc = GetDepthState(newPassData.pShader->GetRenderStates().at(L"Depth"));

			m_passDatas.emplace_back(std::move(newPassData));
		} 
	}

	PassData& Material::GetPassData(UINT passIndex)  noexcept
	{
		return m_passDatas.at(passIndex);
	}

	const UINT Material::FindPassIndex(const std::string& name) const noexcept
	{
		for (auto& passData : m_passDatas)
		{
			if (passData.Name == name)
			{
				return passData.PassIndex;
			}
		}
	}

	bool Material::HasMeshRender() const noexcept
	{
		return m_pMeshRender != nullptr;
	}

	template<typename T>
	void Material::UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const T data)
	{
		static_assert(std::is_trivially_copyable_v<T>, "T must be POD");
		
		const size_t size = sizeof(T);
		if (offset + size > CBuffer.CPUPtr.size())
		{
			throw std::out_of_range("Buffer update exceeds allocated size.");
		} 
		memcpy(CBuffer.CPUPtr.data() + offset, &data, size);
		CBuffer.MakeDirty(offset, size);
	}
	template<typename T>
	void Material::UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const std::vector<T> data)
	{
		auto size = sizeof(T) * data.size();
		if (offset + size > CBuffer.CPUPtr.size())
		{
			throw std::out_of_range("Buffer update exceeds allocated size.");
		}
		memcpy(CBuffer.CPUPtr.data() + offset, data.data(), size);
		CBuffer.MakeDirty(offset, size);
	}

	void Material::SetFloat(const size_t hashName, const float newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		
		auto pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer.get();
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				float currValue = *reinterpret_cast<float*>(cbuffer.CPUPtr.data() + memberData.StartOffset);
				if (currValue != newValue)
				{
					UpdateCBuffer<float>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetInt(const size_t hashName, const int newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				auto currValue = *reinterpret_cast<int*>(cbuffer.CPUPtr.data() + memberData.StartOffset);
				if (currValue != newValue)
				{
					UpdateCBuffer<int>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetUINT(const size_t hashName, const UINT newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				auto currValue = *reinterpret_cast<UINT*>(cbuffer.CPUPtr.data() + memberData.StartOffset);
				if (currValue != newValue)
				{
					UpdateCBuffer<UINT>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	} 
	void Material::SetBool(const size_t hashName, const int newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				auto currValue = *reinterpret_cast<int*>(cbuffer.CPUPtr.data() + memberData.StartOffset);
				if (currValue != newValue)
				{
					UpdateCBuffer<int>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetMatrix(const size_t hashName, const Matrix newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		
		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < NUM_RESOURCE_SPACES; ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];
 
			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				if (cbuffer.CPUPtr.empty() || memberData.StartOffset + sizeof(Matrix) > cbuffer.CPUPtr.size())
				{
					ThrowRuntimeError("invalid size");
					return;
				}
				
				Matrix currValue;
				memcpy(&currValue, cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(Matrix));
				
				if (!EqualFloatBufferWithTolerance(reinterpret_cast<const float*>(&currValue), reinterpret_cast<const float*>(&newValue), 16))
				{
					UpdateCBuffer<Matrix>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetFloatArray(const size_t hashName, const std::vector<float> newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		if (newValue.empty())
		{
			throw std::out_of_range("empty vector");
		}

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				std::vector<float> currValue(newValue.size());
				if (cbuffer.CPUPtr.empty() || memberData.StartOffset + sizeof(float) * newValue.size() > cbuffer.CPUPtr.size())
				{
					return;
				}
				memcpy(currValue.data(), cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(float) * newValue.size());

				if (currValue != newValue)
				{
					UpdateCBuffer<float>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetVector2Array(const size_t hashName, const std::vector<Vector2> newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		if (newValue.empty())
		{
			throw std::out_of_range("empty vector");
		}

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				std::vector<Vector2> currValue(newValue.size());
				if (cbuffer.CPUPtr.empty() || memberData.StartOffset + sizeof(Vector2) * newValue.size() > cbuffer.CPUPtr.size())
				{
					return;
				}

				memcpy(currValue.data(), cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(Vector2) * newValue.size());

				if (currValue != newValue)
				{
					UpdateCBuffer<Vector2>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetVector3Array(const size_t hashName, const std::vector<Vector3> newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		if (newValue.empty())
		{
			throw std::out_of_range("empty vector");
		}

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				std::vector<Vector3> currValue(newValue.size());
				if (cbuffer.CPUPtr.empty() || memberData.StartOffset + sizeof(Vector3) * newValue.size() > cbuffer.CPUPtr.size())
				{
					return;
				}

				memcpy(currValue.data(), cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(Vector3) * newValue.size());

				if (currValue != newValue)
				{
					UpdateCBuffer<Vector3>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
	void Material::SetVector4Array(const size_t hashName, const std::vector<Vector4> newValue, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		if (newValue.empty())
		{
			throw std::out_of_range("empty vector");
		}

		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (!allCBuffers[spaceID].CPUPtr.data()) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				std::vector<Vector4> currValue(newValue.size());
				if (cbuffer.CPUPtr.empty() || memberData.StartOffset + sizeof(Vector3) * newValue.size() > cbuffer.CPUPtr.size())
				{
					return;
				}

				memcpy(currValue.data(), cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(Vector4) * newValue.size());

				if (currValue != newValue)
				{
					UpdateCBuffer<Vector4>(cbuffer, memberData.StartOffset, newValue);
				}
			}
		}
	}
}