#include "Renderer.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace ElysiaRenderer
{
	Renderer::Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize, std::shared_ptr<DX12UI> pUI) :
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
		m_pMeshManager = std::make_unique<MeshManager>();
		m_pTextureManager = std::make_unique<TextureManager>();

		g_vertexShaders = std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>();
		g_pixelShaders = std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>();
		g_computeShaders = std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>();
		
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
		m_pMeshManager->Init();
		
		m_pCameraManager->CreateMainCamera(Vector3(-11.5f, 200.85f, -0.45f) ,
			m_aspectRatio, 3.14159f / 4.0f, 0.1f, 2000.f);

		auto sobolSequence = Create2DSobolSqeuence(64);
		memcpy(RenderResource::GetInstance().GetCBVPassParameter()->sobolSequence.data(), sobolSequence.data(), sobolSequence.size() * sizeof(Vector2));

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

		m_passes.clear();
		m_graphicsContext.release();
		GetVertexShaders().clear();
		GetPixelShaders().clear();
		GetComputeShaders().clear();
		m_graphicsPipelineStates.clear();
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
		UpdatePassCBV();
		UpdateObjectCBV();
	}
	void Renderer::UpdatePassCBV()
	{
		auto& pUserData = UserData::GetInstance();
		auto passParameter = RenderResource::GetInstance().GetCBVPassParameter();

		passParameter->screenSize = GetDevice()->GetScreenSize();
		passParameter->frameIndex = GetDevice()->GetFrameIndex(); 
		passParameter->cameraPosWS = m_pCameraManager->GetMainCamera()->GetPosition4();
		passParameter->viewMatrix = m_pCameraManager->GetMainCamera()->GetViewMat();
		passParameter->viewMatrix_I = passParameter->viewMatrix.Invert();
		passParameter->projMatrix = m_pCameraManager->GetMainCamera()->GetProj();
		passParameter->projMatrix_I = passParameter->projMatrix.Invert();
		passParameter->viewProjMatrix = passParameter->viewMatrix * passParameter->projMatrix;
		passParameter->viewProjMatrix_I = passParameter->viewProjMatrix.Invert();
		passParameter->nearZ = m_pCameraManager->GetMainCamera()->GetNearZ();
		passParameter->farZ = m_pCameraManager->GetMainCamera()->GetFarZ();

		passParameter->mainLight = std::move(GetLightManager()->GetMainLight()->CreateLightData());

		GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(passParameter, sizeof(CBVMainPassParameter));
	}
	void Renderer::UpdateObjectCBV()
	{
		auto& pUserData = UserData::GetInstance();

		for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
			auto objectConstantParameter = meshRenderer.m_CBVObjectParameter.get();
			for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
			{
				objectConstantParameter->hasNormalTex = g_pModelImporter->GetMaterial(meshRenderer.m_mesh->materialIndex).hasNormal;
				objectConstantParameter->baseColorTint = pUserData.BaseColorTint;
				objectConstantParameter->opacity = pUserData.Opacity;
				objectConstantParameter->cutoff = pUserData.Cutoff;
				objectConstantParameter->normalIntensity = pUserData.NormalIntensity;
				objectConstantParameter->metallicIntensity = pUserData.MetallicIntensity;
				objectConstantParameter->roughnessIntensity = pUserData.RoughnessIntensity;
				objectConstantParameter->ambientCubemapIntensity = pUserData.AmbientCubemapIntensity;
				objectConstantParameter->ambientCubemapTint = pUserData.AmbientCubemapTint;

				auto objectContantBuffer = GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, frameIndex, meshIndex);
				objectContantBuffer->SetMappedData(objectConstantParameter, sizeof(CBVObjectParameter));
			}
		}
	}

	void Renderer::Setup()
	{
		RenderPassData passData{};
		passData.RenderSize = UINT2(GetDevice()->GetScreenSize().x, GetDevice()->GetScreenSize().y);
		passData.pCommand = m_graphicsContext.get();
		passData.pGraphicsPipelineStates = &m_graphicsPipelineStates;

		m_passes.emplace_back(std::move(std::make_unique<ShadowPass>()));
		m_passes.emplace_back(std::move(std::make_unique<GBufferPass>()));
		//m_passes.emplace_back(std::move(std::make_unique<OpaquePass>()));


		GetModelImporter()->CreateVertexBuffer();
		GetModelImporter()->CreateIndexBuffer();
		GetModelImporter()->CreateMeshRenders();

 		CreateConstantBuffers();
		for (auto& pass : m_passes)
		{
			pass->Setup(passData);
		}

		LoadTextures();
		LoadShaders();
		CreatePOS();
	}

	void Renderer::LoadShaders()
	{
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"PS", ShaderType::Pixel);
		 
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"VS", ShaderType::Vertex);
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"PS", ShaderType::Pixel);
	}
	void Renderer::CreateConstantBuffers()
	{
		BufferCreationDesc desc{};
		desc.m_accessFlags = BufferAccessFlags::HostWritable;
		desc.m_viewFlags = GPUResourceFlags::CBV;
		desc.m_isRawAccess = false;
		
		desc.m_size = sizeof(CBVMainPassParameter);
		GetBufferManager()->AddConstantBuffer(PER_PASS_SPACE, desc);
		GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(RenderResource::GetInstance().GetCBVPassParameter(), sizeof(CBVMainPassParameter));
		RenderResource::GetPerMainBindResourceSpace()->SetCBV(GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE));
		RenderResource::GetPerMainBindResourceSpace()->Lock();

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
		RenderResource::GetPerObjectBindResourceSpace()->SetCBV(GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, 0, 0));
		RenderResource::GetPerObjectBindResourceSpace()->Lock();
	}

	void Renderer::LoadTextures()
	{
		TextureCreationDesc texBufferCreateDesc{};

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_E_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));
			 
			RenderResource::GetInstance().GetCBVPassParameter()->GGX_E_LUT_Index = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));
		}

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_Eavg_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			RenderResource::GetInstance().GetCBVPassParameter()->GGX_Eavg_LUT_Index = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));

		}
		  
		{
			texBufferCreateDesc.texturePath = L"Tex\\cubemap0.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));
			 
			RenderResource::GetInstance().GetCBVPassParameter()->SkyboxTexIndex = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));
		}

		{
			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
			texBufferCreateDesc.texturePath = StringToWstring(std::filesystem::path(assetsPath).string() + "Tex\\bluenoise_frd_1024x1024.png");
			texBufferCreateDesc.isSRGB = false;

			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			RenderResource::GetInstance().GetCBVPassParameter()->BlueNoiseTexIndex = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));
		}
	}
	void Renderer::CreatePOS()
	{
		PipelineResourceLayout meshResourceLayout{};
		PipelineStateCreateDesc pipelineStateCreateDesc{};
		
		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		/// Opaque PSO
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_inputElementDesc = g_inputElementDescs;
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Opaque][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Opaque][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Enabled);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::BackFaceCull);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		m_graphicsPipelineStates[ShaderQueue::Opaque] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

		/// Skybox PSO
		//meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = m_perObjectBindResourceSpace;
		//meshResourceLayout.m_spaces[PER_PASS_SPACE] = m_perMainPassBindResourceSpace;
		//pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		//pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Skybox][ShaderType::Vertex].get();
		//pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Skybox][ShaderType::Pixel].get();
		//pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		//pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_depthBufferCreateDesc["Camera"].m_resouceDesc.Format;
		//pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		//// skybox not cull back and front
		//pipelineStateCreateDesc.m_rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		//// let cubemap z = 1 pass z-test, otherwise it'll be failed in z-test because data of zbuffer is 1
		//pipelineStateCreateDesc.m_depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//m_graphicsPipelineStates.insert({ ShaderQueue::Skybox, std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout)) });
	}

	void Renderer::AddUIItems()
	{
		auto& pUserData = UserData::GetInstance();

		if (ImGui::CollapsingHeader("Light"))
		{
			auto mainLight = RenderResource::GetInstance().GetCBVPassParameter();
			ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
			ImGui::DragFloat3("Direction", (float*)&pUserData.lightDir, 1, -1, 1);
			ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 5);

			int shadowTypeIndex = (int)pUserData.shadowType;
			ImGui::Combo("Shadow Type", &shadowTypeIndex,
				StringViewToChar(magic_enum::enum_names<ShadowType>().data(), magic_enum::enum_count<ShadowType>()).data(),
				(int)magic_enum::enum_count<ShadowType>());
			pUserData.shadowType = (ShadowType)shadowTypeIndex;


			int shadowQualityIndex = (int)pUserData.shadowQuality;
			ImGui::Combo("Shadow Quality", &shadowQualityIndex, 
				StringViewToChar(magic_enum::enum_names<ShadowQuality>().data(), magic_enum::enum_count<ShadowQuality>()).data(),
				(int)magic_enum::enum_count<ShadowQuality>());
			pUserData.shadowQuality = (ShadowQuality)shadowQualityIndex;

			ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowSlopeDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Max Slope Depth Bias", &pUserData.shadowMaxSlopeDepthBias, 0, 10);
		}

		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&pUserData.BaseColorTint);
			ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f);
			ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f);
			ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &pUserData.AmbientCubemapIntensity, 0.f, 2.f);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&pUserData.AmbientCubemapTint);
		}
	}

	void Renderer::Execute()
	{
		GetDevice()->BeginFrame();
		m_graphicsContext->Reset();
		auto& currBackBuffer = GetDevice()->GetCurrBackBuffer();

		{
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			AddUIItems();
			ImGui::Render();

			m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			//m_graphicsContext->AddBarrier(*GetBufferManager()->GetCameraDepthRT()->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
			m_graphicsContext->FlushBarrier();
			m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_graphicsContext->SetIndexBuffer(GetBufferManager()->GetIndexBufferView());
			m_graphicsContext->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));
		}

		for (auto& pass : m_passes)
		{
			pass->Render();
		}
		DrawOpaque();
		DrawUI();

		{
			m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
			m_graphicsContext->FlushBarrier();

			GetDevice()->SubmitContextWork(*m_graphicsContext);

			GetDevice()->EndFrame();
			GetDevice()->Present();
		}
	} 
	
	void Renderer::DrawOpaque()
	{
		auto& currBackBuffer = GetDevice()->GetCurrBackBuffer();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1, 1, 1));
		//m_graphicsContext->ClearDepthStencilTarget(*GetBufferManager()->GetCameraDepthRT(), 1.f, 0);

		m_graphicsContext->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(GetDevice()->GetScreenSize().x), static_cast<UINT>(GetDevice()->GetScreenSize().y)));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_graphicsPipelineStates[ShaderQueue::Opaque].get();
		pipelineStateData.m_renderTargets.emplace_back(&currBackBuffer);
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			auto texResources = m_pTextureManager->GetTextureResources();
			for (size_t i = 0; i < texResources.size(); ++i)
			{
				if (texResources[i] == nullptr)
				{
					ThrowRuntimeError("nullptr");;
				}
				isReady &= texResources[i]->GetIsReady();
			}
		}
		if (isReady)
		{
			m_graphicsContext->SetPipeline(pipelineStateData);
			m_graphicsContext->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, GetDevice()->GetFrameID(), meshIndex);
				RenderResource::GetPerObjectBindResourceSpace()->SetCBV(objectContantBuffer);
				m_graphicsContext->SetPipelineResource(PER_OBJECT_SPACE, RenderResource::GetPerObjectBindResourceSpace());

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_graphicsContext->Draw(indexCount, startVertex, startIndex);
			}
		}
	}
	void Renderer::DrawSkybox()
	{
		/*auto& currBackBuffer = m_device->GetCurrBackBuffer();

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Skybox].get(),
			std::vector<DX12TextureResource*>{ &currBackBuffer },
			m_pCameraDepthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);

		{
			SetPipelineResource(m_objectCBVIndex, CBVPassParameterType::Main);
			DrawCommand(m_objectCBVIndex++);
		}*/
	}
	void Renderer::DrawUI()
	{
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_graphicsContext->GetCommandList());
	}
} 