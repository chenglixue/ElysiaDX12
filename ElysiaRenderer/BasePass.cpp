#include "stdafx.h"
#include "BasePass.h"

#include "PIXHelper.h"
#include "RenderTexture.h"
#include "RenderResource.h"
#include "TextureManager.h"
#include "Common.h"

namespace ElysiaRenderer
{
	BasePass::BasePass(DX12Camera* pCamera) :
		m_renderSize(0, 0),
		m_pCamera(pCamera),
		m_shaderPasses(std::vector<ShaderPass>()),
		m_pMaterial(std::unique_ptr<ElysiaRenderer::RenderMaterial>()),
		m_PipelineStateObjects(std::unordered_map<UINT, PipelineStateObject*>())
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

		//pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		//pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
		//pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Blit][ShaderType::Pixel].get();
		//pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
		//pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
		//pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		//pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
		//pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		Configure();
	}

	void BasePass::Dispose()
	{
	}

}