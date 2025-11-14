#include "stdafx.h"
#include "Shader.h"

#include "DX12Device.h"
#include "DX12BufferResource.h"
#include "PipelineResourceUtility.h"

namespace ElysiaRenderer
{
	Shader::Shader(std::vector<ShaderPass>& shaderPasses) :
		m_shaderVariables(std::unordered_map<std::string, ShaderVariable>()),
		m_constantVariableDescs(std::unordered_map<std::string, ShaderConstantVariableDesc>()),
		m_passDatas(std::unordered_map<std::string, PassData>())
	{
		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			auto newPassData = PassData();

			// set shader pass
			newPassData.PassIndex = passID;

			// create dx12 shader
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

						auto pNewBuffer = std::move(GetDevice()->CreateBuffer(bufferDesc));

						std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
						pPipelineResourceSpace->SetCBV(pNewBuffer.release());
						pPipelineResourceSpace->Lock();

						newPassData.MeshResourceLayouts->m_spaces[currVariable.spaceID] = pPipelineResourceSpace.release();

						break;
					}
				}
			}

			newPassData.BlendDesc = shaderPasses[passID].BlendDesc;
			newPassData.RasterizerDesc = shaderPasses[passID].RasterizerDesc;
			newPassData.DepthStencilDesc = shaderPasses[passID].DepthStencilDesc;

			m_passDatas.insert({shaderPasses[passID].Name, std::move(newPassData)});
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

		return std::move(PassData());
	}

	const PassData& Shader::GetPassData(std::string passName) const noexcept
	{
		if (m_passDatas.contains(passName))
		{
			return m_passDatas.at(passName);
		}

		ThrowRuntimeError("Null Pass Data");

		return std::move(PassData());
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
	void Shader::SetConstantVariable(const std::string name, T data)
	{
		auto& desc = m_constantVariableDescs[name];
		memcpy(desc.pData, &data, desc.Size);

		for (auto& passData : m_passDatas)
		{
			passData.second.MeshResourceLayouts->m_spaces[desc.SpaceID]->GetCBV()->SetDirty(true);
		}
	}

	void Shader::ApplyConstantData()
	{
		for (auto& constantVariableDesc : m_constantVariableDescs)
		{
			auto& desc = constantVariableDesc.second;

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

	template void Shader::SetConstantVariable<UINT>(const std::string, UINT);
	template void Shader::SetConstantVariable<int>(const std::string, int);
	template void Shader::SetConstantVariable<float>(const std::string, float);
	template void Shader::SetConstantVariable<Vector2>(const std::string, Vector2);
	template void Shader::SetConstantVariable<Vector3>(const std::string, Vector3);
	template void Shader::SetConstantVariable<Vector4>(const std::string, Vector4);
	template void Shader::SetConstantVariable<Matrix>(const std::string, Matrix);
	template void Shader::SetConstantVariable<bool>(const std::string, bool);
	template void Shader::SetConstantVariable<std::vector<Vector2>>(const std::string, std::vector<Vector2>);
}