#include "stdafx.h"
#include "Shader.h"

#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12BufferResource.h"
#include "lib/Utility/PipelineResourceUtility.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	Shader::Shader(std::vector<ShaderPass>& shaderPasses) :
		m_passDatas()
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
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
					},
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Pixel,
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
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

			newPassData.pShader = GetDevice()->CreateShader(desc);

			newPassData.MeshResourceLayouts = std::make_unique<PipelineResourceLayout>();
			// set bind resource for rootsignature
			for (auto& shaderVariable : m_shaderVariables)
			{
				auto currVariable = shaderVariable.second;
				switch (currVariable.type)
				{
					case ShaderVariable::Type::ConstantBuffer:
					{
						BufferCreationDesc bufferDesc
						{
							.m_name = stringToLPCWSTR(currVariable.name),
							.m_size = currVariable.size,
							.m_viewFlags = GPUResourceFlags::CBV,
							.m_accessFlags = BufferAccessFlags::HostWritable,
							.m_isRawAccess = false,
						};

						if (currVariable.spaceID == PER_OBJECT_SPACE)
						{
							newPassData.ObjectBufferDesc = bufferDesc;
						}
						else if (currVariable.spaceID == PER_MATERIAL_SPACE)
						{
							newPassData.MaterialBufferDesc = bufferDesc;
						}
						
						{
							auto pNewBuffer = std::move(GetDevice()->CreateBuffer(bufferDesc));

							std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
							pPipelineResourceSpace->SetCBV(pNewBuffer.release());
							pPipelineResourceSpace->Lock();

							newPassData.MeshResourceLayouts->m_spaces[currVariable.spaceID] = pPipelineResourceSpace.release();
						}

						break;
					}
				}
			}

			newPassData.BlendDesc = shaderPasses[passID].BlendDesc;
			newPassData.RasterizerDesc = shaderPasses[passID].RasterizerDesc;
			newPassData.DepthStencilDesc = shaderPasses[passID].DepthStencilDesc;

			m_passDatas.emplace_back(std::move(newPassData));
		}

	}

	const PassData& Shader::GetPassData(UINT passIndex) const noexcept
	{
		if (m_passDatas.size() > passIndex)
		{
			return m_passDatas.at(passIndex);
		}
		else
		{
			ThrowRuntimeError("No suitable passData");
		}
	}

	const UINT Shader::FindPassIndex(const std::string& name) const noexcept
	{
		for(size_t passIndex = 0; passIndex < m_passDatas.size(); ++passIndex)
		{
			if(m_passDatas[passIndex].Name == name)
			{
				return static_cast<UINT>(passIndex);
			}
		}

		ThrowRuntimeError("invalid pass index");
	}

	template<typename T>
	void Shader::SetConstantVariable(const std::string& name, const T data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto hash = PropertyToID(name);
		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}

	template<typename T>
	void Shader::SetConstantVariable(const size_t hash, const T data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}

	void Shader::ApplyConstantData()
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);

		for (size_t i = 0; i < m_passDatas.size(); i++)
		{
			auto& passData = m_passDatas[i];
			auto& constantVariableDescs = m_constantVariableDescs[i];

			for(auto& pair : constantVariableDescs)
			{
				auto& desc = pair.second;
				
				if (!desc.IsDirty) continue;

				auto meshResourceLayouts = passData.MeshResourceLayouts.get();
				auto buffer = meshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->GetMappedBuffer();
				buffer += desc.StartOffset;
				assert(buffer != nullptr && desc.pData.data() != nullptr && desc.Size > 0);
				memcpy(buffer, desc.pData.data(), desc.Size);
				desc.IsDirty = false;
			}
		}
	}

	template void Shader::SetConstantVariable<UINT>(const std::string&, const UINT, UINT);
	template void Shader::SetConstantVariable<int>(const std::string&, const int, UINT);
	template void Shader::SetConstantVariable<float>(const std::string&, const float, UINT);
	template void Shader::SetConstantVariable<Vector2>(const std::string&, const Vector2, UINT);
	template void Shader::SetConstantVariable<Vector3>(const std::string&, const Vector3, UINT);
	template void Shader::SetConstantVariable<Vector4>(const std::string&, const Vector4, UINT);
	template void Shader::SetConstantVariable<Matrix>(const std::string&, const Matrix, UINT);
	template void Shader::SetConstantVariable<math::Matrix4>(const std::string&, math::Matrix4, UINT passID);
	template<> void Shader::SetConstantVariable<bool>(const std::string& name, const bool data, UINT passID)
	{
		SetConstantVariable(name, (UINT)(data == true ? 1 : 0), passID);
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector2>>(const std::string& name, const std::vector<Vector2> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto hash = PropertyToID(name);
		auto constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector3>>(const std::string& name, const std::vector<Vector3> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto hash = PropertyToID(name);
		auto constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector4>>(const std::string& name, const std::vector<Vector4> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto hash = PropertyToID(name);
		auto constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<UINT>>(const std::string& name, const std::vector<UINT> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto hash = PropertyToID(name);
		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}

	template void Shader::SetConstantVariable<UINT>(const size_t hash, const UINT, UINT);
	template void Shader::SetConstantVariable<int>(const size_t hash, const int, UINT);
	template void Shader::SetConstantVariable<float>(const size_t hash, const float, UINT);
	template void Shader::SetConstantVariable<Vector2>(const size_t hash, const Vector2, UINT);
	template void Shader::SetConstantVariable<Vector3>(const size_t hash, const Vector3, UINT);
	template void Shader::SetConstantVariable<Vector4>(const size_t hash, const Vector4, UINT);
	template void Shader::SetConstantVariable<Matrix>(const size_t hash, const Matrix, UINT);
	template void Shader::SetConstantVariable<math::Matrix4>(const size_t hash, math::Matrix4, UINT passID);
	template<> void Shader::SetConstantVariable<bool>(const size_t hash, const bool data, UINT passID)
	{
		SetConstantVariable(hash, (UINT)(data == true ? 1 : 0), passID);
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector2>>(const size_t hash, const std::vector<Vector2> data , UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector3>>(const size_t hash, const std::vector<Vector3> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<Vector4>>(const size_t hash, const std::vector<Vector4> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<UINT>>(const size_t hash, const std::vector<UINT> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto& constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto& constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}
}
