#pragma once
#include "stdafx.h"

namespace ElysiaCore
{
	class DX12Device;
	class DX12GraphicsContext;
}

namespace ElysiaRenderer
{
	class RenderTexture;
}

namespace ElysiaRenderer
{
	struct RenderPassData
	{
		Vector2 RenderSize = Vector2::One;
		DX12Device* pDevice = nullptr;
		DX12GraphicsContext* pCommand = nullptr;

		RenderTexture* pCameraColorRT = nullptr;
		RenderTexture* pCameraDepthRT = nullptr;
	};
}