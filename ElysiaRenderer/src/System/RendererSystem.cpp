#include "stdafx.h"
#include "RendererSystem.h"

#include <dxgidebug.h>

#include "lib/DX12//DX12UI.h"
#include "src/Manager/BufferManager.h"
#include "src/Manager/LightManager.h"
#include "src/Manager/CameraManager.h"
#include "lib/Event/Messager.h"

#include "src/Pass/ShadowPass.h"
#include "src/Pass/GBufferPass.h"
#include "src/Pass/AOPass.h"
#include "src/Pass/OpaquePass.h"
#include "src/Pass/TonemapPass.h"
#include "src/Pass/UIPass.h"
#include "src/Pass/FinalBlitPass.h"
#include "src/Pass/BloomPass.h"

#include "lib/Utility/SobolSequenceGenerator.h"
#include "src/Parameter/CBVParameter.h"
#include "RenderResource.h"
#include "DX12/UploadRingBuffer.h"
#include "Manager/RenderTargetManager.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	RendererSystem::RendererSystem(HWND windowHandle, UINT2 screenSize, DX12UI* pUI) :
		m_pUI(pUI),
		m_disableLocalDimming(false),
		m_displayModesAvailable(),
		m_displayModesNamesAvailable(),
		m_currentDisplayMode(DISPLAYMODE_SDR),
		m_VsyncEnabled(false)
	{
		m_windowHandle = windowHandle;
		m_aspectRatio = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);
		m_pDevice = std::make_unique<DX12Device>(windowHandle, screenSize);
		m_graphicsContext = m_pDevice->CreateGraphicsContext();

		DeSerializeUserData();
		
		m_pDevice->EnumerateDisplayModes(&m_displayModesAvailable, &m_displayModesNamesAvailable);

		
		m_pUI->InitDescriptor(windowHandle, m_pDevice.get());
		TextureManager::GetInstance().Init(m_pDevice.get());
		RenderTargetManager::GetInstance().Init(m_pDevice.get());
		CameraManager::GetInstance().Init(m_pDevice.get());
		LightManager::GetInstance().Init(m_pDevice.get());
		BufferManager::GetInstance().Init(m_pDevice.get());
		PSOManager::GetInstance().Init(m_pDevice.get());
		
		g_pModelImporter = std::make_unique<ModelImporter>(m_pDevice.get());
	}

	RendererSystem::~RendererSystem()
	{
	}

	void RendererSystem::Init()
	{
#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
		Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
		if (FAILED(initialize))
			// error
#else
		HRESULT hr = ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		if (FAILED(hr))
			// error
#endif

		printf("loading...\n");
		if (!GetModelImporter()->Load(g_ModelPaths))
		{
			AssertError("failed to load model: %s\n");
		}
		printf("done\n");

		UpdateDisplay(UserData::GetInstance().displayMode, m_disableLocalDimming);
		
		InitPSOHelpers();  

		CameraManager::GetInstance().CreateMainCamera(Vector3(-11.5f, 200.85f, -0.45f) ,
			m_aspectRatio, 3.14159f / 4.0f, 0.1f, 2000.f);

		Setup();
	}
	void RendererSystem::Update()
	{
		if (CheckIfWindowModeHdrOn() && (m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_SDR ||
						m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_HDR10_2084 ||
						m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_HDR10_SCRGB))
		{
			UpdateDisplay(UserData::GetInstance().displayMode, m_disableLocalDimming);
		}
		//OnKeyboardInput();
		SerializeUserData();
	}
	void RendererSystem::Render()
	{
		Execute();
	}
	void RendererSystem::Destory()
	{
		m_pDevice->WaitForIdle();
		m_pDevice->DestoryContext(std::move(m_graphicsContext));

		m_graphicsContext.release();
	}
	void RendererSystem::Resize()
	{

	}

	void RendererSystem::OnMouseDown(WPARAM btnState, int x, int y)
	{
		m_lastMousePos = XMINT2(x, y);

		// Capture mouse input to the specified window. 
		// This means that even if the mouse pointer moves out of the window, 
		// all subsequent mouse messages (such as WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_RBUTTONDOWN, etc.) will still be sent to that window 
		// until the ReleaseCapture function is called to release the capture.
		SetCapture(m_windowHandle);
	}
	void RendererSystem::OnMouseUp(WPARAM btnState, int x, int y)
	{
		ReleaseCapture();
	}
	void RendererSystem::OnMouseMove(WPARAM btnState, int x, int y)
	{
		m_lastMousePos.x = x;
		m_lastMousePos.y = y;
	}
	void RendererSystem::OnKeyboardInput()
	{
	}

	void RendererSystem::Execute()
	{
		m_pDevice->BeginFrame();
		m_graphicsContext->Reset();

		if(BufferManager::GetInstance().GetVertexBuffer()->GetIsReady() && BufferManager::GetInstance().GetIndexBuffer()->GetIsReady())
		{
			for (auto& pass : m_passes)
			{
				pass->Render();
			}
		}

		m_pDevice->SubmitContextWork(*m_graphicsContext);
		m_pDevice->EndFrame();

		m_pDevice->Present(); 
	}

	void RendererSystem::Setup()
	{
		GetModelImporter()->CreateVertexBuffer();
		GetModelImporter()->CreateIndexBuffer();
		GetModelImporter()->CreateMeshRenders();

		RenderPassData passData
		{
			.RenderSize = m_pDevice->GetScreenSize().xy(),
			.pDevice = m_pDevice.get(), 
			.pCommand = m_graphicsContext.get(),
		};

		m_passes.emplace_back(std::move(std::make_unique<ShadowPass>(CameraManager::GetInstance().GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<GBufferPass>(CameraManager::GetInstance().GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<AOPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<OpaquePass>(CameraManager::GetInstance().GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<TonemapPass>(m_pCameraManager->GetMainCamera())));
		// m_passes.emplace_back(std::move(std::make_unique<BloomPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<UIPass>()));
		m_passes.emplace_back(std::move(std::make_unique<FinalBlitPass>(CameraManager::GetInstance().GetMainCamera())));
		for (auto& pass : m_passes)
		{ 
			pass->Setup(passData);
		}
	}
	void RendererSystem::UpdateDisplay(int displayMode, bool disableLocalDimming)
	{
		// Nothing was changed in UI
		if (displayMode < 0)
		{
			m_currentDisplayModeNamesIndex = m_previousDisplayModeNamesIndex;
			return;
		}

		if (m_currentDisplayMode != displayMode || m_disableLocalDimming != disableLocalDimming)
		{
			// Flush GPU
			m_pDevice->WaitForIdle();

			m_currentDisplayMode = (DisplayMode)displayMode;
			m_disableLocalDimming = disableLocalDimming;

			m_pDevice->OnCreateWindowSizeDependentResources(m_pDevice->GetScreenSize().x, m_pDevice->GetScreenSize().y, m_VsyncEnabled, m_currentDisplayMode, m_disableLocalDimming);
		}
	}
}            