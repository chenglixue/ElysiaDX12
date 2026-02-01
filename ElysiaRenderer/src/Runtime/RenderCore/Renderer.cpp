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
        // initialize the GPU time stamps module
        m_pGPUTimer->OnCreate(pDevice, NUM_BACK_BUFFERS);

        InitPSOHelpers();

        m_passes.clear();
        m_passes.emplace_back(
            std::move(std::make_unique<PreDrawPass>()));
        m_passes.emplace_back(
            std::move(std::make_unique<ShadowPass>()));
        m_passes.emplace_back(
            std::move(std::make_unique<GBufferPass>()));
        m_passes.emplace_back(std::move(std::make_unique<AOPass>()));
        m_passes.emplace_back(
            std::move(std::make_unique<OpaquePass>()));
        m_passes.emplace_back(
            std::move(std::make_unique<TonemapPass>()));
        // m_passes.emplace_back(std::move(std::make_unique<BloomPass>(m_pCameraManager->GetMainCamera())));
        m_passes.emplace_back(std::move(std::make_unique<DebugPass>()));
        m_passes.emplace_back(std::move(std::make_unique<UIPass>()));
        m_passes.emplace_back(std::move(
            std::make_unique<FinalBlitPass>()));
    }

    void Renderer::OnCreateWindowSizeDependentResources(SwapChain* pSwapChain,
                                                        uint32_t Width,
                                                        uint32_t Height)
    {
        m_Width = Width;
        m_Height = Height;
        m_viewport = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f,
                      1.0f};
        m_rectScissor = {0, 0, (LONG)Width, (LONG)Height};

        CameraManager::GetInstance().CreateMainCamera(
            Vector3(-11.5f - 1000.f, 200.85f, -0.45f) * 0.01f,
            static_cast<float>(Width) / static_cast<float>(Height),
            AMD_PI_OVER_4,
            0.1f,
            1000.f);

        if (!UserData::GetInstance().IsUseHDR)
        {
            m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                Width,
                Height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                true,
                L"Camera Color RT");
        }
        else
        {
            switch (UserData::GetInstance().HDRLevel)
            {
            case HDRQuality::Low:
            {
                m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    Width,
                    Height,
                    DXGI_FORMAT_R11G11B10_FLOAT,
                    true,
                    L"Camera Color RT");
                break;
            }
            case HDRQuality::High:
            {
                m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                    Width,
                    Height,
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    true,
                    L"Camera Color RT");
                break;
            }
            default:
            {
                ThrowRuntimeError("Invalid choose");
                break;
            }
            }
        }
        m_pCameraDepthRT = RenderTargetManager::GetInstance().CreateRenderTexture(Width,
                                                                                  Height,
                                                                                  DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                                                                                  true,
                                                                                  L"Camera Depth RT");

        RenderPassData passData
        {
            .RenderSize = {m_Width, m_Height},
            .pDevice = m_pDevice,
            .pCommand = m_pGraphicsContext,
            .pSwapChain = pSwapChain,
            .pCameraColorRT = m_pCameraColorRT,
            .pCameraDepthRT = m_pCameraDepthRT
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
        LightManager::GetInstance().Update(frameContext);
        SerializeUserData();

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
}