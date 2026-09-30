#pragma once

#include "Programs/public/Helper.h"
#include "Runtime/RenderCore/Pass/public/BasePass.h"
#include "Runtime/Developer/public/GPUTimestamps.h"
#include "Runtime/Engine/public/FrameContext.h"

namespace ElysiaEditor
{
    class IMGUIDrawer;
    class DX12UI;
}

namespace ElysiaCore
{
    class SwapChain;
    struct PipelineStateObject;
    class DX12TextureResource;
}

namespace ElysiaRenderer
{
    class MeshRenderer;
    class MeshManager;
    class TextureManager;
    class CameraManager;
}

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;
    using namespace ElysiaCore;
    using namespace ElysiaEditor;

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        void OnCreateWindowSizeDependentResources(SwapChain* pSwapChain,
                                                  uint32_t Width,
                                                  uint32_t Height);
        void OnDestroyWindowSizeDependentResources();
        void OnUpdateDisplayDependentResources(SwapChain* pSwapChain);

        // Shadow map resolution changed (shadow quality): let the passes that own
        // shadow-sized resources rebuild just those, instead of recreating every
        // window-sized resource.
        void RefreshShadowDependentResources();

        void OnCreate(DX12Device* pDevice,
                      SwapChain* pSwapChain,
                      ElysiaCore::DX12GraphicsContext* context);
        void OnRender(ElysiaEngine::FrameContext frameContext);
        void OnDestory();
        void OnUpdateConstantBuffer(std::vector<RenderItem>& renderList);

        const std::vector<TimeStamp>& GetTimingValues()
        {
            return m_TimeStamps;
        }

        RenderTexture* GetDisplayRT() const
        {
            return m_pDisplayRT;
        }

    protected:
        DX12Device* m_pDevice = nullptr;
        DX12GraphicsContext* m_pGraphicsContext = nullptr;
        uint32_t m_Width;
        uint32_t m_Height;
        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_rectScissor;

        LARGE_INTEGER cpuStart;
        LARGE_INTEGER cpuEnd;

        std::unique_ptr<GPUTimestamps> m_pGPUTimer = nullptr;
        std::vector<TimeStamp> m_TimeStamps;

        std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
        std::vector<std::unique_ptr<BasePass>> m_passes{};
        eastl::vector<std::unique_ptr<MeshRenderer>> m_meshRenderers;
        RenderTexture* m_pCameraColorRT = nullptr;
        RenderTexture* m_pCameraDepthRT = nullptr;
        RenderTexture* m_pDisplayRT = nullptr;

        // Warms every runtime-switchable shader keyword combination inside the
        // PSO precache batch, so toggling those settings later is a cache lookup
        // instead of a synchronous PSO creation (which causes a hitch).
        void PrecacheKeywordCombinations();

        template <typename T, typename... Args>
        Renderer& AddPass(Args&&... args)
        {
            m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            return *this; // 返回自身以支持链式调用
        }
    };
}