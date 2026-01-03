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
    class DX12Camera;
}

namespace ElysiaRenderer
{
    struct RenderPassData
    {
        Vector2 RenderSize = Vector2::One;
        DX12Device* pDevice = nullptr;
        DX12GraphicsContext* pCommand = nullptr;
        SwapChain* pSwapChain;

        RenderTexture* pCameraColorRT = nullptr;
        RenderTexture* pCameraDepthRT = nullptr;
    };
}