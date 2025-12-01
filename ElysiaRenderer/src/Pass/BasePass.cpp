#include "stdafx.h"
#include "BasePass.h"

#include "lib/Utility/PIXHelper.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
#include "Manager/TextureManager.h"
#include "lib/Utility/Common.h"

namespace ElysiaRenderer
{
	BasePass::BasePass(DX12Camera* pCamera) :
		m_renderSize(Vector2::Zero),
		m_pCamera(pCamera),
		m_shaderPasses(),
		m_pMaterial(),
		m_PipelineStateObjects(),
		m_enableKeywords()
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