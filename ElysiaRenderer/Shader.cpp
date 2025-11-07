#include "stdafx.h"
#include "Shader.h"

#include "DX12Device.h"
#include "DX12Shader.h"
#include "PipelineResourceUtility.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	Shader::Shader(std::vector<ShaderPass>& shaderPasses) : 
		m_shaderPassIDs(std::unordered_map<std::string, UINT>())
	{
		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			// set shader pass
			m_shaderPassIDs[shaderPasses[passID].Name] = passID;

			// create dx12 shader
			ShaderCreateDesc vertexShaderCreateDesc{};
			ZeroMemory(&vertexShaderCreateDesc, sizeof(ShaderCreateDesc));
			vertexShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
			vertexShaderCreateDesc.entryPoint = shaderPasses[passID].VertexEntryPoint;
			vertexShaderCreateDesc.shaderType = ShaderType::Vertex;
			shaderPasses[passID].pVSShader = std::move(GetDevice()->CreateShader(vertexShaderCreateDesc));

			ShaderCreateDesc fragmentShaderCreateDesc{};
			ZeroMemory(&fragmentShaderCreateDesc, sizeof(ShaderCreateDesc));
			fragmentShaderCreateDesc.shaderName = shaderPasses[passID].FilePath;
			fragmentShaderCreateDesc.entryPoint = shaderPasses[passID].FragmentEntryPoint;
			fragmentShaderCreateDesc.shaderType = ShaderType::Pixel;
			shaderPasses[passID].pPSShader = std::move(GetDevice()->CreateShader(fragmentShaderCreateDesc));

			// shader reflect
			for (auto& VSShaderVariable : shaderPasses[passID].pVSShader->GetVariable())
			{
				m_shaderVariables[VSShaderVariable.name] = VSShaderVariable;
			}
			for (auto& PSShaderVariable : shaderPasses[passID].pPSShader->GetVariable())
			{
				if (m_shaderVariables.find(PSShaderVariable.name) != m_shaderVariables.end()) continue;
				m_shaderVariables[PSShaderVariable.name] = PSShaderVariable;
			}

			for (auto& VSConstantVariableDesc : shaderPasses[passID].pVSShader->GetConstantBufferVariables())
			{
				m_constantVariableDescs.insert(std::move(VSConstantVariableDesc));
			}
			for (auto& PSConstantVariableDesc : shaderPasses[passID].pPSShader->GetConstantBufferVariables())
			{
				if (m_constantVariableDescs.find(PSConstantVariableDesc.first) != m_constantVariableDescs.end()) continue;
				m_constantVariableDescs.insert(std::move(PSConstantVariableDesc));
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
						m_meshResourceLayouts[shaderPasses[passID].Name] = std::move(pPipelineResourceLayout);

						break;
					}
				}


			}

			// create PSO
			auto pPipelineStateObject = std::make_unique<PipelineStateObject>();

			PipelineStateCreateDesc pipelineStateCreateDesc{};
			pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = shaderPasses[passID].pVSShader.get();
			pipelineStateCreateDesc.m_pixelShader = shaderPasses[passID].pPSShader.release();
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 0;
			pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
			pipelineStateCreateDesc.m_rasterDesc = shaderPasses[passID].RasterizerDesc;
			pipelineStateCreateDesc.m_blendDesc = shaderPasses[passID].BlendDesc;
			pipelineStateCreateDesc.m_depthStencilDesc = shaderPasses[passID].DepthStencilDesc;
			pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		}

	}
}