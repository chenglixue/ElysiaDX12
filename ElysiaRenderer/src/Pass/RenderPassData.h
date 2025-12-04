#pragma once
#include "stdafx.h"
#include "lib/DX12//DX12GraphicsContext.h"

namespace ElysiaRenderer
{
	struct RenderPassData
	{
		Vector2 RenderSize = Vector2::One;
		DX12Device* pDevice = nullptr;
		DX12GraphicsContext* pCommand = nullptr;
	};
}