#include "stdafx.h"
#include "Shader.h"

#include "DX12Device.h"
#include "DX12BufferResource.h"
#include "PipelineResourceUtility.h"

namespace ElysiaRenderer
{
	Shader::Shader(std::vector<ShaderPass>& shaderPasses) : 
		m_passDatas(std::unordered_map<std::string, PassData>())
	{
		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			auto newPassData = PassData();
			ZeroMemory(&newPassData, sizeof(PassData));

			// set shader pass
			newPassData.PassIndex = passID;

			// create dx12 shader
			ShaderCreateDesc vertexShaderCreateDesc{};
			ZeroMemory(&vertexShaderCreateDesc, sizeof(ShaderCreateDesc));
			vertexShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
			vertexShaderCreateDesc.entryPoint = shaderPasses[passID].VertexEntryPoint;
			vertexShaderCreateDesc.shaderType = ShaderType::Vertex;
			newPassData.pVSShader = std::move(GetDevice()->CreateShader(vertexShaderCreateDesc));

			ShaderCreateDesc fragmentShaderCreateDesc{};
			ZeroMemory(&fragmentShaderCreateDesc, sizeof(ShaderCreateDesc));
			fragmentShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
			fragmentShaderCreateDesc.entryPoint = shaderPasses[passID].FragmentEntryPoint;
			fragmentShaderCreateDesc.shaderType = ShaderType::Pixel;
			newPassData.pPSShader = std::move(GetDevice()->CreateShader(fragmentShaderCreateDesc));

			// shader reflect
			for (auto& VSShaderVariable : newPassData.pVSShader->GetVariable())
			{
				auto emplaceResult = m_shaderVariables.try_emplace(VSShaderVariable.name);
				if (emplaceResult.second)
				{
					emplaceResult.first->second = VSShaderVariable;
				}
			}
			for (auto& PSShaderVariable : newPassData.pPSShader->GetVariable())
			{
				auto emplaceResult = m_shaderVariables.try_emplace(PSShaderVariable.name);
				if (emplaceResult.second)
				{
					emplaceResult.first->second = PSShaderVariable;
				}
			}

			for (auto& VSConstantVariableDesc : newPassData.pVSShader->GetConstantBufferVariables())
			{
				auto emplaceResult = m_constantVariableDescs.try_emplace(VSConstantVariableDesc.first);
				if (emplaceResult.second)
				{
					emplaceResult.first->second = VSConstantVariableDesc.second;
				}
			}
			for (auto& PSConstantVariableDesc : newPassData.pPSShader->GetConstantBufferVariables())
			{
				auto emplaceResult = m_constantVariableDescs.try_emplace(PSConstantVariableDesc.first);
				if (emplaceResult.second)
				{
					emplaceResult.first->second = PSConstantVariableDesc.second;
				}
			}

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

						auto pNewBuffer = std::move(GetDevice()->CreateBuffer(bufferDesc));

						std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
						pPipelineResourceSpace->SetCBV(pNewBuffer.release());
						pPipelineResourceSpace->Lock();

						auto pPipelineResourceLayout = std::make_unique<PipelineResourceLayout>();
						pPipelineResourceLayout->m_spaces[currVariable.spaceID] = pPipelineResourceSpace.release();
						newPassData.MeshResourceLayouts = std::move(pPipelineResourceLayout);

						break;
					}
				}
			}

			m_passDatas[shaderPasses[passID].Name] = std::move(newPassData);
		}

	}

	const PassData& Shader::GetPassData(UINT passIndex) const noexcept
	{
		for (auto& passData : m_passDatas)
		{
			if (passData.second.PassIndex == passIndex)
			{
				return m_passDatas.at(passData.first);
			}
		}
		
		ThrowRuntimeError("No suitable passData");

		return PassData();
	}

	const PassData& Shader::GetPassData(std::string passName) const noexcept
	{
		if (m_passDatas.contains(passName))
		{
			return m_passDatas.at(passName);
		}

		ThrowRuntimeError("Null Pass Data");

		return PassData();
	}

	UINT Shader::FindPassIndex(std::string passName) const noexcept
	{
		if (m_passDatas.contains(passName))
		{
			return m_passDatas.at(passName).PassIndex;
		}

		ThrowRuntimeError("Invalid Pass Index");

		return -1;
	}

	template<typename T>
	void Shader::SetConstantVariable(const std::string& name, T data)
	{
		auto desc = m_constantVariableDescs[name];
		memcpy(desc[name].pData, &data, desc.Size);

		for (auto& passData : m_passDatas)
		{
			passData.second.MeshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->SetDirty(true);
		}
	}

	void Shader::ApplyConstantData()
	{
		for (auto& constantVariableDesc : m_constantVariableDescs)
		{
			auto desc = constantVariableDesc.second;

			for (auto& passData : m_passDatas)
			{
				auto meshResourceLayouts = passData.second.MeshResourceLayouts.get();
				if(!(meshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->GetIsDirty()))
				{
					break;
				}

				auto buffer = reinterpret_cast<char*>(meshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->GetMappedBuffer());
				buffer += desc.StartOffset;
				memcpy(buffer, desc.pData, desc.Size);
			}
		}
	}
}