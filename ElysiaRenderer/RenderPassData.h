#pragma once
#include "stdafx.h"
#include "DX12GraphicsContext.h"

namespace ElysiaRenderer
{
	struct RenderPassData
	{
		Vector2 RenderSize = Vector2::One;
		DX12GraphicsContext* pCommand;
	};
}