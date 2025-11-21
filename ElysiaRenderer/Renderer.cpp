#include "stdafx.h"
#include "Renderer.h"

#include <dxgidebug.h>

#include "DX12UI.h"
#include "BufferManager.h"
#include "LightManager.h"
#include "CameraManager.h"
#include "ShaderManager.h"

#include "ShadowPass.h"
#include "GBufferPass.h"
#include "AOPass.h"
#include "OpaquePass.h"
#include "TonemapPass.h"
#include "UIPass.h"
#include "FinalBlitPass.h"

#include "SobolSequenceGenerator.h"
#include "CBVParameter.h"
#include "RenderResource.h"


extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	Renderer::Renderer(HWND windowHandle, UINT2 screenSize, std::shared_ptr<DX12UI> pUI) :
		m_pUI(pUI)
	{
		m_windowHandle = windowHandle;
		m_aspectRatio = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);
		g_device = std::make_unique<DX12Device>(windowHandle, screenSize);
		m_graphicsContext = GetDevice()->CreateGraphicsContext();

		m_pUI->InitDescriptor(windowHandle, std::move(GetDevice()));
		m_pCameraManager = std::make_unique<CameraManager>();
		g_pLightManager = std::make_unique<LightManager>();
		g_pBufferManager = std::make_unique<BufferManager>();
		m_pTextureManager = std::make_unique<TextureManager>();
		g_pShaderManager = std::make_unique<ShaderManager>();
		g_pRenderResource = std::make_unique<RenderResource>();
		g_pPSOManager = std::make_unique<PSOManager>();
		
		g_pModelImporter = std::make_unique<ModelImporter>(GetBufferManager(), m_pTextureManager.get());
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Init()
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

		DeSerializeUserData();

		InitPSOHelpers();  

		m_pCameraManager->Init(); 
		GetLightManager()->Init();
		GetBufferManager()->Init();
		m_pTextureManager->Init();
		GetShaderManager()->Init();
		
		m_pCameraManager->CreateMainCamera(Vector3(-11.5f, 200.85f, -0.45f) ,
			m_aspectRatio, 3.14159f / 4.0f, 0.1f, 2000.f);

		Setup();
	}
	void Renderer::Update()
	{
		//OnKeyboardInput();
		GetLightManager()->Update();
		UpdateCBV();
		SerializeUserData();
	}
	void Renderer::Render()
	{
		Execute();
	}
	void Renderer::Destory()
	{
		GetDevice()->WaitForIdle();
		GetDevice()->DestoryContext(std::move(m_graphicsContext));

		m_graphicsContext.release();
	}
	void Renderer::Resize()
	{

	}

	void Renderer::OnMouseDown(WPARAM btnState, int x, int y)
	{
		m_lastMousePos = XMINT2(x, y);

		// Capture mouse input to the specified window. 
		// This means that even if the mouse pointer moves out of the window, 
		// all subsequent mouse messages (such as WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_RBUTTONDOWN, etc.) will still be sent to that window 
		// until the ReleaseCapture function is called to release the capture.
		SetCapture(m_windowHandle);
	}
	void Renderer::OnMouseUp(WPARAM btnState, int x, int y)
	{
		ReleaseCapture();
	}
	void Renderer::OnMouseMove(WPARAM btnState, int x, int y)
	{
		m_lastMousePos.x = x;
		m_lastMousePos.y = y;
	}
	void Renderer::OnKeyboardInput()
	{
	}

	void Renderer::UpdateCBV()
	{
		auto passParameter = GetRenderResource()->GetCBVFrameVariable();
		passParameter->cameraPosWS = m_pCameraManager->GetMainCamera()->GetPosition4();
		passParameter->lightData = std::move(GetLightManager()->GetMainLight()->CreateLightData());
		passParameter->frameIndex = GetDevice()->GetFrameIndex();
		passParameter->nearZ = m_pCameraManager->GetMainCamera()->GetNearZ();
		passParameter->farZ = m_pCameraManager->GetMainCamera()->GetFarZ();
		passParameter->ZBufferParams = Vector4(1 - m_pCameraManager->GetMainCamera()->GetFarZ() / m_pCameraManager->GetMainCamera()->GetNearZ(),
			m_pCameraManager->GetMainCamera()->GetFarZ() / m_pCameraManager->GetMainCamera()->GetNearZ(),
			(1 - m_pCameraManager->GetMainCamera()->GetFarZ() / m_pCameraManager->GetMainCamera()->GetNearZ()) / m_pCameraManager->GetMainCamera()->GetFarZ(),
			(m_pCameraManager->GetMainCamera()->GetFarZ() / m_pCameraManager->GetMainCamera()->GetNearZ()) / m_pCameraManager->GetMainCamera()->GetFarZ());
		GetBufferManager()->GetSingleConstantBuffer(PER_FRAME_SPACE)->SetMappedData(GetRenderResource()->GetCBVFrameVariable(), sizeof(CBVFrameVariable));
	}

	void Renderer::Setup()
	{
		GetModelImporter()->CreateVertexBuffer();
		GetModelImporter()->CreateIndexBuffer();
		GetModelImporter()->CreateMeshRenders();

 		CreateConstantBuffers();

		RenderPassData passData{};  
		passData.RenderSize = GetDevice()->GetScreenSize().xy();
		passData.pCommand = m_graphicsContext.get();

		m_passes.emplace_back(std::move(std::make_unique<ShadowPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<GBufferPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<AOPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<OpaquePass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<TonemapPass>(m_pCameraManager->GetMainCamera())));
		m_passes.emplace_back(std::move(std::make_unique<UIPass>()));
		m_passes.emplace_back(std::move(std::make_unique<FinalBlitPass>(m_pCameraManager->GetMainCamera())));
		for (auto& pass : m_passes)
		{ 
			pass->Setup(passData);
		}
	}
	void Renderer::CreateConstantBuffers()
	{
		BufferCreationDesc desc{};
		desc.m_accessFlags = BufferAccessFlags::HostWritable;
		desc.m_viewFlags = GPUResourceFlags::CBV;
		desc.m_isRawAccess = false;
		
		desc.m_size = sizeof(CBVObjectParameter);
		for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);

			GetBufferManager()->AddConstantBuffer(PER_OBJECT_SPACE, desc);

			auto objectConstantParameter = *meshRenderer.m_CBVObjectParameter;
			for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
			{
				auto objectContantBuffer = GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, frameIndex, meshIndex);
				objectContantBuffer->SetMappedData(&objectConstantParameter, sizeof(CBVObjectParameter));
			}
		}

		desc.m_size = sizeof(CBVFrameVariable);
		GetBufferManager()->AddConstantBuffer(PER_FRAME_SPACE, desc);
		GetRenderResource()->GetPerFrameBindResourceSpace()->SetCBV(GetBufferManager()->GetSingleConstantBuffer(PER_FRAME_SPACE));
		GetRenderResource()->GetPerFrameBindResourceSpace()->Lock();
	}

	void Renderer::Execute()
	{
		GetDevice()->BeginFrame();
		m_graphicsContext->Reset();

		//PIXHelper pix(m_graphicsContext->GetCommandList(), "Deferred Render");
		for (auto& pass : m_passes)
		{
			pass->Render();
		}

		{
			GetDevice()->SubmitContextWork(*m_graphicsContext);

			GetDevice()->EndFrame();
			GetDevice()->Present();
		}
	} 
}            