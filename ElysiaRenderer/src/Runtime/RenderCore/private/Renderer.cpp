#include "stdafx.h"
#include "../public/Renderer.h"

#include <dxgidebug.h>

#include "Editor/public/UserData.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Device.h"

#include "Runtime/Resource/Model/public/ModelManager.h"

#include "../public/MeshRenderer.h"
#include "../public/BufferManager.h"
#include "../public/LightManager.h"
#include "../public/CameraManager.h"

#include "Runtime/RenderCore/Pass/public/RenderPassData.h"
#include "Runtime/RenderCore/Pass/public/PreDrawPass.h"
#include "Runtime/RenderCore/Pass/public/ShadowPass.h"
#include "Runtime/RenderCore/Pass/public/GBufferPass.h"
#include "Runtime/RenderCore/Pass/public/AOPass.h"
#include "Runtime/RenderCore/Pass/public/OpaquePass.h"
#include "Runtime/RenderCore/Pass/public/TonemapPass.h"
#include "Runtime/RenderCore/Pass/public/UIPass.h"
#include "Runtime/RenderCore/Pass/public/FinalBlitPass.h"
#include "Runtime/RenderCore/Pass/public/BloomPass.h"

#include "../public/RenderTargetManager.h"
#include "../public/TonemapUtility.h"
#include "Editor/public/IMGUIDrawer.h"
#include "Runtime/RenderCore/Pass/public/BakePass.h"
#include "Runtime/RenderCore/Pass/public/DebugPass.h"
#include "Runtime/RenderCore/Pass/public/GIPass.h"
#include "Runtime/RenderCore/Pass/public/SharpenPass.h"
#include "Runtime/RenderCore/Pass/public/SelectionOutlinePass.h"
#include "Runtime/RenderCore/Pass/public/SkyboxPass.h"
#include "Runtime/RenderCore/Pass/public/TAAPass.h"
#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/RenderCore/Pass/public/ShadowProjectionPass.h"
#include "Runtime/RenderCore/Pass/public/SSSRPass.h"
#include "Runtime/RenderCore/public/PSOManager.h"

extern "C"
{
__declspec(dllexport) extern const UINT D3D12SDKVersion = 618;
}

extern "C"
{
__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

namespace ElysiaRenderer
{
    using namespace ElysiaModel;
    using namespace ElysiaCore;

    Renderer::Renderer() = default;
    Renderer::~Renderer() = default;

    void Renderer::OnCreate(DX12Device* pDevice,
                            SwapChain* pSwapChain,
                            DX12GraphicsContext* context)
    {
        m_pDevice = pDevice;
        m_pGraphicsContext = context;
        m_pGPUTimer = std::make_unique<GPUTimestamps>();
        m_pGPUTimer->OnCreate(pDevice, NUM_BACK_BUFFERS);

        InitPSOHelpers();

        m_passes.clear();

        AddPass<PreDrawPass>();
        AddPass<BakePass>();
        AddPass<ShadowPass>();
        AddPass<GIPass>();
        AddPass<GBufferPass>();
        AddPass<AOPass>();
        AddPass<ShadowProjectionPass>();
        AddPass<OpaquePass>();
        AddPass<SkyboxPass>();
        AddPass<SSSRPass>();
        AddPass<TAAPass>();
        AddPass<BloomPass>();
        AddPass<TonemapPass>();
        AddPass<SharpenPass>();
        AddPass<SelectionOutlinePass>();
        AddPass<DebugPass>();
        AddPass<UIPass>();
        AddPass<FinalBlitPass>();
    }

    void Renderer::OnCreateWindowSizeDependentResources(SwapChain* pSwapChain,
                                                        uint32_t Width,
                                                        uint32_t Height)
    {
        m_Width = std::floor(Width * UserData::GetInstance().taaParameter.sampleRate);
        m_Height = std::floor(Height * UserData::GetInstance().taaParameter.sampleRate);

        m_viewport = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f,
                      1.0f};
        m_rectScissor = {0, 0, (LONG)Width, (LONG)Height};

        if (!UserData::GetInstance().hdrParameter.IsUseHDR)
        {
            m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                m_Width,
                m_Height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                true,
                L"Camera Color RT");
            m_pDisplayRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                Width,
                Height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                true,
                L"Display RT");
        }
        else
        {
            switch (UserData::GetInstance().hdrParameter.HDRLevel)
            {
            case HDRQuality::Low:
            {
                m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    m_Width,
                    m_Height,
                    DXGI_FORMAT_R11G11B10_FLOAT,
                    true,
                    L"Camera Color RT");
                m_pDisplayRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    Width,
                    Height,
                    DXGI_FORMAT_R11G11B10_FLOAT,
                    true,
                    L"Display RT");
                break;
            }
            case HDRQuality::High:
            {
                m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    m_Width,
                    m_Height,
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    true,
                    L"Camera Color RT");
                m_pDisplayRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    Width,
                    Height,
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    true,
                    L"Display RT");
                break;
            }
            default:
            {
                ThrowRuntimeError("Invalid choose");
                break;
            }
            }
        }
        m_pCameraDepthRT = RenderTargetManager::GetInstance().CreateRenderTexture(m_Width,
                                                                                  m_Height,
                                                                                  DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                                                                                  true,
                                                                                  L"Camera Depth RT");

        RenderPassData passData
        {
            .RenderSize = {Width, Height},
            .pDevice = m_pDevice,
            .pCommand = m_pGraphicsContext,
            .pSwapChain = pSwapChain,
            .pCameraColorRT = m_pCameraColorRT,
            .pCameraDepthRT = m_pCameraDepthRT,
            .pDisplayRT = m_pDisplayRT
        };

        PSOManager::GetInstance().BeginPrecacheBatch();
        for (auto& pass : m_passes)
        {
            pass->Setup(passData);
        }
        PrecacheKeywordCombinations();
        PSOManager::GetInstance().WaitPrecacheBatch();
    }

    void Renderer::OnDestroyWindowSizeDependentResources()
    {

    }

    void Renderer::OnUpdateDisplayDependentResources(SwapChain* pSwapChain)
    {
        for (auto& pass : m_passes)
        {
            pass->UpdatePipeline();
        }
    }

    void Renderer::RefreshShadowDependentResources()
    {
        for (auto& pass : m_passes)
        {
            pass->OnShadowResolutionChanged();
        }
    }

    void Renderer::OnRender(ElysiaEngine::FrameContext frameContext)
    {
        static LARGE_INTEGER frequency = {};
        if (frequency.QuadPart == 0)
        {
            QueryPerformanceFrequency(&frequency);
        }
        QueryPerformanceCounter(&cpuStart);

        LightManager::GetInstance().Update(frameContext);
        SerializeUserData();

        OnUpdateConstantBuffer(frameContext.renderList);

        QueryPerformanceCounter(&cpuEnd);
        float elapsedUs = (float)((cpuEnd.QuadPart - cpuStart.QuadPart) * 1000000 / frequency.QuadPart);
        m_pGPUTimer->GetTimeStampUser({"CPU/RenderPrepare", elapsedUs});

        frameContext.pGPUTimer = m_pGPUTimer.get();

        UINT64 gpuTicksPerSecond;
        m_pDevice->GetDirectQueue()->GetTimestampFrequency(&gpuTicksPerSecond);
        m_pGPUTimer->OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);
        m_pGPUTimer->GetTimeStamp(m_pGraphicsContext->GetCommandList(), "Begin Frame");

        for (auto& pass : m_passes)
        {
            pass->Render(frameContext);
        }

        m_pGPUTimer->OnEndFrame();
        m_pGPUTimer->CollectTimings(m_pGraphicsContext->GetCommandList());
    }

    void Renderer::OnDestory()
    {
        m_pGPUTimer->OnDestroy();
    }

    void Renderer::PrecacheKeywordCombinations()
    {
        // Shadow quality / type select shader keywords (SHADOW_QUALITY_* and
        // HARD_SHADOW / SOFT_SHADOW) consumed by OpaquePass, ShadowPass and
        // ShadowProjectionPass. Setup() only resolves the *currently selected*
        // combination, so switching these settings at runtime used to create the
        // missing PSOs synchronously inside a frame and hitch. Warm every
        // combination here instead - still inside the precache batch, so the
        // work runs on the precache threads and, from the second run on, is
        // satisfied from the PSO disk cache.
        auto& shadowParameter = UserData::GetInstance().shadowParameter;
        const ShadowQuality savedQuality = shadowParameter.shadowQuality;
        const ShadowType savedType = shadowParameter.shadowType;

        constexpr ShadowQuality qualities[] =
        {
            ShadowQuality::Low,
            ShadowQuality::Middle,
            ShadowQuality::High,
            ShadowQuality::VeryHigh
        };
        constexpr ShadowType types[] = {ShadowType::Hard, ShadowType::Soft};

        for (const ShadowQuality quality : qualities)
        {
            for (const ShadowType type : types)
            {
                if (quality == savedQuality && type == savedType)
                {
                    continue; // already warmed by Setup()
                }

                shadowParameter.shadowQuality = quality;
                shadowParameter.shadowType = type;

                // Passes re-resolve their keyword set from UserData and create
                // (or look up) the matching PSOs.
                for (auto& pass : m_passes)
                {
                    pass->UpdatePipeline();
                }
            }
        }

        // Restore the user's selection and rebuild the active pipelines.
        shadowParameter.shadowQuality = savedQuality;
        shadowParameter.shadowType = savedType;

        for (auto& pass : m_passes)
        {
            pass->UpdatePipeline();
        }
    }

    void Renderer::OnUpdateConstantBuffer(std::vector<ElysiaRenderer::RenderItem>& renderList)
    {
        for (auto& ri : renderList)
        {
            Entity* entity = ri.pAssociatedEntity;
            if (!entity)
                continue;

            if (entity->IsDirty())
            {
                ri.NumFramesDirty = NUM_FRAMES_IN_FLIGHT;
                entity->ClearDirty();
            }

            if (ri.NumFramesDirty > 0)
            {
                ri.worldMatrix = entity->transform.GetWorldMatrix();

                ri.NumFramesDirty --;
            }
        }
    }
}