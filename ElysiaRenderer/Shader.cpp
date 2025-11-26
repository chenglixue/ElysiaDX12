#include "stdafx.h"
#include "Shader.h"

#include "DX12Device.h"
#include "DX12BufferResource.h"
#include "PipelineResourceUtility.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	Shader::Shader(std::vector<ShaderPass>& shaderPasses) :
		m_shaderVariables(),
		m_constantVariableDescs(),
		m_passDatas()
	{
		m_passDatas.reserve(shaderPasses.size());
		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			auto newPassData = PassData();

			// set shader pass
			newPassData.PassIndex = passID;

			bool hasCS = shaderPasses[passID].IsComputeShader;
			
			// create dx12 shader
			if (!hasCS)
			{
				ShaderCreateDesc vertexShaderCreateDesc{};
				vertexShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
				vertexShaderCreateDesc.entryPoint = shaderPasses[passID].VertexEntryPoint;
				vertexShaderCreateDesc.shaderType = ShaderType::Vertex;
				newPassData.pVSShader = std::move(GetDevice()->CreateShader(vertexShaderCreateDesc));

				ShaderCreateDesc fragmentShaderCreateDesc{};
				fragmentShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
				fragmentShaderCreateDesc.entryPoint = shaderPasses[passID].FragmentEntryPoint;
				fragmentShaderCreateDesc.shaderType = ShaderType::Pixel;
				newPassData.pPSShader = std::move(GetDevice()->CreateShader(fragmentShaderCreateDesc));
			}
			else
			{
				ShaderCreateDesc computeShaderCreateDesc{};
				computeShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
				computeShaderCreateDesc.entryPoint = shaderPasses[passID].ComputeEntryPoint;
				computeShaderCreateDesc.shaderType = ShaderType::Compute;
				newPassData.pCSShader = std::move(GetDevice()->CreateShader(computeShaderCreateDesc));
			}

			// shader reflect
			if (hasCS)
			{
				if (newPassData.pCSShader)
				{
					for (auto& CSShaderVariable : newPassData.pCSShader->GetVariable())
					{
						auto emplaceResult = m_shaderVariables.try_emplace(CSShaderVariable.name);
						if (emplaceResult.second)
						{
							emplaceResult.first->second = CSShaderVariable;
						}
					}
				}
			}
			else
			{
				if (newPassData.pVSShader)
				{
					for (auto& VSShaderVariable : newPassData.pVSShader->GetVariable())
					{
						auto emplaceResult = m_shaderVariables.try_emplace(VSShaderVariable.name);
						if (emplaceResult.second)
						{
							emplaceResult.first->second = VSShaderVariable;
						}
					}

				}
				if (newPassData.pPSShader)
				{
					for (auto& PSShaderVariable : newPassData.pPSShader->GetVariable())
					{
						auto emplaceResult = m_shaderVariables.try_emplace(PSShaderVariable.name);
						if (emplaceResult.second)
						{
							emplaceResult.first->second = PSShaderVariable;
						}
					}

				}
			}

			m_constantVariableDescs.emplace(passID, std::unordered_map<size_t, ShaderConstantVariableDesc>{});
			if (hasCS)
			{
				if (newPassData.pCSShader)
				{
					for (auto& CSConstantVariableDesc : newPassData.pCSShader->GetConstantBufferVariables())
					{
						auto hash = PropertyToID(CSConstantVariableDesc.first);

						auto& passMap = m_constantVariableDescs[passID];
						passMap.emplace(hash, CSConstantVariableDesc.second);
					}
				}
			}
			else
			{
				if (newPassData.pVSShader)
				{
					for (auto& VSConstantVariableDesc : newPassData.pVSShader->GetConstantBufferVariables())
					{
						auto hash = PropertyToID(VSConstantVariableDesc.first);

						auto emplaceresult = m_constantVariableDescs[passID].try_emplace(hash);
						if (emplaceresult.second)
						{
							emplaceresult.first->second = VSConstantVariableDesc.second;
						}
					}

				}
				if (newPassData.pPSShader)
				{
					for (auto& PSConstantVariableDesc : newPassData.pPSShader->GetConstantBufferVariables())
					{
						auto hash = PropertyToID(PSConstantVariableDesc.first);

						auto emplaceresult = m_constantVariableDescs[passID].try_emplace(hash);
						if (emplaceresult.second)
						{
							emplaceresult.first->second = PSConstantVariableDesc.second;
						}
					}

				}
			}

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

	template<typename T>
	void Shader::SetConstantVariable(const std::string& name, const T data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto hash = PropertyToID(name);
		auto constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto constantVariableDesc = constantVariableDescMap.at(hash);
			memcpy(constantVariableDesc.pData.data(), pSourceData, constantVariableDesc.Size);

			constantVariableDesc.IsDirty = true;
		}
	}

	template<typename T>
	void Shader::SetConstantVariable(const size_t hash, const T data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto constantVariableDescMap = m_constantVariableDescs.at(passID);
		if (constantVariableDescMap.contains(hash))
		{
			auto constantVariableDesc = constantVariableDescMap.at(hash);
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

			for(auto constantVariableDesc : constantVariableDescs)
			{
				if (constantVariableDesc.second.)
			}

			for (auto& passData : m_passDatas)
			{
				if (passData.second.PassIndex != desc.PassID) continue;
				if (!desc.IsDirty) continue;

				auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();

				auto buffer = meshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->GetMappedBuffer();
				buffer += desc.StartOffset;
				assert(buffer != nullptr && desc.pData.data() != nullptr && desc.Size > 0);
				memcpy(buffer, desc.pData.data(), desc.Size);

			}
			desc.IsDirty = true;
		}

		for (auto& constantVariableDesc : m_constantVariableDescs)
		{
			auto& desc = constantVariableDesc.second;

			for (auto& passData : m_passDatas)
			{
				if (passData.second.PassIndex != desc.PassID) continue;
				if (!desc.IsDirty) continue;

				auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();

				auto buffer = meshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->GetMappedBuffer();
				buffer += desc.StartOffset;
				assert(buffer != nullptr && desc.pData.data() != nullptr && desc.Size > 0);
				memcpy(buffer, desc.pData.data(), desc.Size);

			}
			desc.IsDirty = true;
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
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size / 4);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
		}
	}
	template void Shader::SetConstantVariable<std::vector<Vector2>>(const std::string&, const std::vector<Vector2>, UINT);
	template void Shader::SetConstantVariable<std::vector<Vector3>>(const std::string&, const std::vector<Vector3>, UINT);
	template<> void Shader::SetConstantVariable<std::vector<Vector4>>(const std::string& name, const std::vector<Vector4> data, UINT passID)
	{
		if(data.data() == nullptr) return;
		
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<UINT>>(const std::string& name, const std::vector<UINT> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
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
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = &data;

		auto name = GetRenderResource()->GetShaderConstantVariable(hash);
		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size / 4);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
		}
	}
	template void Shader::SetConstantVariable<std::vector<Vector2>>(const size_t hash, const std::vector<Vector2>, UINT);
	template void Shader::SetConstantVariable<std::vector<Vector3>>(const size_t hash, const std::vector<Vector3>, UINT);
	template<> void Shader::SetConstantVariable<std::vector<Vector4>>(const size_t hash, const std::vector<Vector4> data, UINT passID)
	{
		if(data.data() == nullptr) return;
		
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto name = GetRenderResource()->GetShaderConstantVariable(hash);
		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
		}
	}
	template<> void Shader::SetConstantVariable<std::vector<UINT>>(const size_t hash, const std::vector<UINT> data, UINT passID)
	{
		std::lock_guard<std::mutex> lockGuard(m_setDataMutex);
		const void* pSourceData = data.data();

		auto name = GetRenderResource()->GetShaderConstantVariable(hash);
		auto itr = m_constantVariableDescs.equal_range(name);
		for (auto currItr = itr.first; currItr != itr.second; ++currItr)
		{
			if (currItr->second.PassID == passID)
			{
				memcpy(currItr->second.pData.data(), pSourceData, currItr->second.Size);

				for (auto& passData : m_passDatas)
				{
					if (passData.second.PassIndex != passID) continue;

					auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
					currItr->second.IsDirty = true;
				}
			}
		}
	}
}
