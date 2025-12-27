#include "stdafx.h"
#include "Renderer.h"

#include <dxgidebug.h>

#include "Editor/DX12UI.h"
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

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace ElysiaRenderer
{
	using namespace ElysiaModel;
	using namespace ElysiaCore;

	Renderer::Renderer() = default;
	Renderer::~Renderer() = default;

	void Renderer::OnCreate(DX12Device* pDevice, SwapChain* pSwapChain)
	{
		m_pDevice = pDevice;
		m_pImGui = std::make_unique<IMGUIDrawer>();
		
		m_graphicsContext = m_pDevice->CreateGraphicsContext();
		// initialize the GPU time stamps module
		m_GPUTimer.OnCreate(pDevice, NUM_BACK_BUFFERS);
		m_pImGui->OnCreate(pDevice, pSwapChain);

		InitPSOHelpers();

		RenderPassData passData
		{
			.RenderSize = {m_Width, m_Height},
			.pDevice = m_pDevice, 
			.pCommand = m_graphicsContext.get(),
			.pSwapChain = pSwapChain,
			.pCameraColorRT = m_pCameraColorRT,
			.pCameraDepthRT = m_pCameraDepthRT
		};

		m_passes.emplace_back(std::move(std::make_unique<PreDrawPass>(CameraManager::GetInstance().GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<ShadowPass>(CameraManager::GetInstance().GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<GBufferPass>(CameraManager::GetInstance().GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<AOPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<OpaquePass>(CameraManager::GetInstance().GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<TonemapPass>(m_pCameraManager->GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<BloomPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<FinalBlitPass>(CameraManager::GetInstance().GetMainCamera())));
		for (auto& pass : m_passes)
		{ 
			pass->Setup(passData);
		}
	}

	void Renderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height)
	{
		m_Width = Width;
		m_Height = Height;
		m_viewport = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
		m_rectScissor = { 0, 0, (LONG)Width, (LONG)Height };
		
		CameraManager::GetInstance().CreateMainCamera(Vector3(-11.5f, 200.85f, -0.45f),
			static_cast<float>(Width) / static_cast<float>(Height),
			3.14159f / 4.0f, 0.1f, 2000.f);

		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(Width, Height,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				true,
				"Camera Color RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
			case HDRQuality::Low: 
				{
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(Width, Height,
						DXGI_FORMAT_R11G11B10_FLOAT,
						true,
						"Camera Color RT"); 
					break;  
				} 
			case HDRQuality::High:
				{ 
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(Width, Height,
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						true,
						"Camera Color RT");
					break;
				}
			default:
				{ 
					ThrowRuntimeError("Invalid choose");
					break;
				}
			}
		}
	}
	void Renderer::OnDestroyWindowSizeDependentResources()
	{
		
	}
	void Renderer::OnUpdateDisplayDependentResources(SwapChain* pSwapChain)
	{
		
	}

	void Renderer::OnRender(ElysiaEngine::FrameContext frameContext)
	{
		m_graphicsContext->Reset();

		LightManager::GetInstance().Update(frameContext);
		SerializeUserData();

		// Timing values
		UINT64 gpuTicksPerSecond;
		m_pDevice->GetDirectQueue()->GetTimestampFrequency(&gpuTicksPerSecond);
		m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);
		m_GPUTimer.GetTimeStamp(m_graphicsContext->GetCommandList(), "Begin Frame");
		
		for (auto& pass : m_passes)
		{
			pass->Render(frameContext);
		}
		
		m_pDevice->SubmitContextWork(*m_graphicsContext, frameContext.frameID);

		m_GPUTimer.OnEndFrame();
	}
	void Renderer::OnDestory()
	{
		m_graphicsContext.release();
	}
}            