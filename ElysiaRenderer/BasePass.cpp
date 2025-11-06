#include "stdafx.h"
#include "BasePass.h"

#include "PIXHelper.h"
#include "RenderTexture.h"
#include "RenderResource.h"
#include "TextureManager.h"
#include "Common.h"


namespace ElysiaRenderer
{
	BasePass::BasePass() :
		m_renderSize(0, 0),
		m_pGraphicsPipelineStates(std::unordered_map<UINT, std::unique_ptr<PipelineStateObject>>()),
		m_shaderVariables(std::unordered_map<std::string, ShaderVariable>()),
		m_meshResourceLayout(PipelineResourceLayout()),
		m_constantVariableDescs(std::unordered_map<std::string, ShaderConstantVariableDesc>())
	{

	}

	BasePass::~BasePass()
	{
		Dispose();
	}

	void BasePass::Setup(const RenderPassData& renderPassData)
	{
		m_renderSize = renderPassData.RenderSize;
		m_pCommand = renderPassData.pCommand;

		if (m_pGraphicsPipelineStates[ShaderQueue::Blit] == nullptr)
		{
			PipelineResourceLayout meshResourceLayout{};
			PipelineStateCreateDesc pipelineStateCreateDesc{};

			meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
			meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

			pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
			pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Blit][ShaderType::Pixel].get();
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
			pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
			pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
			pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
			pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			m_pGraphicsPipelineStates[ShaderQueue::Blit] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

		}

		Configure();
	}

	void BasePass::Dispose()
	{
		m_pGraphicsPipelineStates.clear();
		m_shaderVariables.clear();
		m_constantVariableDescs.clear();
	}

	void BasePass::SetConstantData(const std::string& name, const void* pData)
	{
		auto desc = m_constantVariableDescs[name];
		memcpy(m_constantVariableDescs[name].pData, pData, desc.Size);

		m_meshResourceLayout.m_spaces[desc.SpaceID]->GetCBV()->SetDirty(true);
	}

	void BasePass::ApplyConstantData()
	{
		for (auto& constantVariableDesc : m_constantVariableDescs)
		{
			auto desc = constantVariableDesc.second;
			if (!m_meshResourceLayout.m_spaces[desc.SpaceID]->GetCBV()->GetIsDirty())
			{
				continue;
			}

			auto buffer = reinterpret_cast<char*>(m_meshResourceLayout.m_spaces[desc.SpaceID]->GetCBV()->GetMappedBuffer());
			buffer += desc.StartOffset;
			memcpy(buffer, desc.pData, desc.Size);
		}
	}
}