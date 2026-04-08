#include "stdafx.h"
#include "Renderer.h"

#include <dxgidebug.h>

#include "Editor/UserData.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Device.h"

#include "Runtime/Resource/Model/ModelManager.h"

#include "MeshRenderer.h"
#include "BufferManager.h"
#include "LightManager.h"
#include "CameraManager.h"

#include "Pass/RenderPassData.h"
#include "Pass/PreDrawPass.h"
#include "Pass/ShadowPass.h"
#include "Pass/GBufferPass.h"
#include "Pass/AOPass.h"
#include "Pass/OpaquePass.h"
#include "Pass/TonemapPass.h"
#include "Pass/UIPass.h"
#include "Pass/FinalBlitPass.h"
#include "Pass/BloomPass.h"

#include "RenderTargetManager.h"
#include "TonemapUtility.h"
#include "Editor/IMGUIDrawer.h"
#include "Pass/DebugPass.h"
#include "Pass/GIPass.h"
#include "Pass/SharpenPass.h"
#include "Pass/SkyboxPass.h"
#include "Pass/TAAPass.h"
#include "Runtime/Engine/ECS/Entity.h"
#include "Pass/ShadowProjectionPass.h"

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
        AddPass<ShadowPass>();
        AddPass<GIPass>();
        AddPass<GBufferPass>();
        AddPass<AOPass>();
        AddPass<ShadowProjectionPass>();
        AddPass<OpaquePass>();
        AddPass<SkyboxPass>();
        AddPass<TAAPass>();
        AddPass<BloomPass>();
        AddPass<TonemapPass>();
        AddPass<SharpenPass>();
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

        for (auto& pass : m_passes)
        {
            pass->Setup(passData);
        }
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