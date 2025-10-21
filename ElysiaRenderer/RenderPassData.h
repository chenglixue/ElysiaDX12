#pragma once
#include "stdafx.h"
#include "DX12GraphicsContext.h"

namespace ElysiaRenderer
{
	struct RenderPassData
	{
		UINT2 RenderSize = UINT2(0, 0);
		DX12GraphicsContext* pCommand;
		std::unordered_map<UINT, std::shared_ptr<PipelineStateObject>> *pGraphicsPipelineStates = nullptr;
	};
}