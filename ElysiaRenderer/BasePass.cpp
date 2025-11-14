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
		m_renderSize(Vector2::Zero),
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

		Configure();
	}

	void BasePass::Dispose()
	{
	}

}