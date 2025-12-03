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
	
	Material::Material(std::vector<ShaderPass>& shaderPasses)
	{
		Init(shaderPasses);
	}

	Material::Material(std::vector<ShaderPass>& shaderPasses, MeshRender* pMeshRender) :
		m_pMeshRender(pMeshRender)
	{
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

			newPassData.pShader = std::move(GetDevice()->CreateShader(desc));
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

	void Material::CreateMaterialCBuffer(size_t passIndex)
	{
		auto layouts = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData.cbuffers;
		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		
		pMaterialCBuffer = std::make_unique<MaterialRuntimeCBuffer>();
		
		for (auto&[space, layout] : layouts)
		{
			RuntimeCBuffer runtimeCBuffer{};
			runtimeCBuffer.CPUPtr.resize(layout.size);

			pMaterialCBuffer->CBuffers[space] = runtimeCBuffer;
		}
	}

	void Material::SetPipelineResourceLayout(PipelineResourceLayout* pPipelineResourceLayout)
	{
		if (HasMeshRender())
		{
			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				auto& meshRenderer = m_pMeshRender[meshIndex];

				for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
				{
					auto objectBufferDesc = pPipelineResourceLayout->m_spaces[PER_OBJECT_SPACE]->GetCBVDesc();
					if (meshRenderer.m_objectBuffers[frameIndex] &&
						meshRenderer.m_objectBuffers[frameIndex]->GetResourceDesc().Width != objectBufferDesc.m_size)
					{
						meshRenderer.m_objectBuffers[frameIndex].reset();
						meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));
					}
					else if(meshRenderer.m_objectBuffers[frameIndex] == nullptr)
					{
						meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));
					}

					auto materialBufferDesc = pPipelineResourceLayout->m_spaces[PER_MATERIAL_SPACE]->GetCBVDesc();
					if (meshRenderer.m_materialBuffers[frameIndex] &&
						meshRenderer.m_materialBuffers[frameIndex]->GetResourceDesc().Width != materialBufferDesc.m_size)
					{
						meshRenderer.m_materialBuffers[frameIndex].reset();
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
					else if(meshRenderer.m_materialBuffers[frameIndex] == nullptr)
					{
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
				}
			}
		}
	}
	
	void Material::SetMaterialCBufferGPUPtr(UINT spaceID, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());

		auto& passData = GetPassData(passIndex);
		auto  GPUPtr = passData.pCurrVariantData->MeshResourceLayouts->m_spaces[spaceID]->GetCBV()->GetMappedBuffer();
		assert(GPUPtr);
		m_passDatas[passIndex].pMaterialCBuffer->CBuffers[spaceID].GPUPtr = GPUPtr;
	}

	void Material::Flush()
	{
		for (auto& passData : m_passDatas)
		{
			for (auto& [spaceID, CBuffer] : passData.pMaterialCBuffer->CBuffers)
			{
				if (!CBuffer.HasDirtyRange()) continue;

				UINT32 size = CBuffer.DirtyEnd - CBuffer.DirtyBegin;
				assert(CBuffer.DirtyBegin + size <= CBuffer.CPUPtr.size());

				memcpy(CBuffer.GPUPtr + CBuffer.DirtyBegin, CBuffer.CPUPtr.data() + CBuffer.DirtyBegin, size);

				CBuffer.ClearDirty();
			}
		}

	}

	template<typename T>
	void Material::UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const T data)
	{
		if (offset + sizeof(T) > CBuffer.CPUPtr.size())
		{
			throw std::out_of_range("Buffer update exceeds allocated size.");
		}
		memcpy(CBuffer.CPUPtr.data() + offset, &data, sizeof(T));
		CBuffer.MakeDirty(offset, sizeof(T));
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
		
		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		assert(pMaterialCBuffer);

		auto& allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (allCBuffers.count(spaceID) == 0) continue;
			auto& cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);

				Matrix currValue;
				memcpy(&currValue, cbuffer.CPUPtr.data() + memberData.StartOffset, sizeof(Matrix));
				if (currValue != newValue)
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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
			if (allCBuffers.count(spaceID) == 0) continue;
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