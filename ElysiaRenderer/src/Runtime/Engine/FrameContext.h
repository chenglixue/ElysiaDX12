#pragma once
#include "Runtime/Developer/GPUTimestamps.h"
#include "Runtime/RenderCore/RenderItem.h"

namespace ElysiaRenderer
{
    class RenderTexture;
    class DX12Camera;
}

namespace ElysiaEngine
{
    struct FrameContext
    {
        UINT frameID;
        UINT64 frameIndex;
        std::vector<ElysiaRenderer::RenderItem> renderList;
        std::function<void()> buildUI;
        ElysiaRenderer::DX12Camera* pCamera;
        ElysiaHelper::GPUTimestamps* pGPUTimer;
        RenderTexture* pResolveRT = nullptr;
    };
}