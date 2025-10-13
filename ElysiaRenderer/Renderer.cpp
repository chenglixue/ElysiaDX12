#include "Renderer.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace ElysiaRenderer
{
	Renderer::Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize, std::shared_ptr<DX12UI> pUI) :
		m_pUI(pUI)
	{
		m_render = this;
		m_windowHandle = windowHandle;
		m_aspectRatio = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);
		m_device = std::make_unique<DX12Device>(windowHandle, screenSize);
		m_graphicsContext = m_device->CreateGraphicsContext();
		

		m_pUI->InitDescriptor(windowHandle, std::move(m_device.get()));
		m_pCameraManager = std::make_unique<CameraManager>();
		m_pLightManager = std::make_unique<LightManager>();
		m_pShadowManager = std::make_unique<ShadowManager>(m_device.get());
		m_pBufferManager = std::make_unique<BufferManager>(m_device.get());
		m_pRenderSource = std::make_unique<RenderResource>(m_device.get());
		m_pMeshManager = std::make_unique<MeshManager>();
		m_pTextureManager = std::make_unique<TextureManager>();
		
		m_pModelImporter = std::make_unique<ModelImporter>(m_device.get(), m_pBufferManager.get(), m_pTextureManager.get());
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
		if (!m_pModelImporter->Load(g_ModelPaths))
		{
			AssertError("failed to load model: %s\n");
		}
		printf("done\n");

		DeSerializeUserData();

		m_pCameraManager->Init(); 
		m_pLightManager->Init();  
		m_pShadowManager->Init();
		m_pBufferManager->Init();
		m_pMeshManager->Init();
		
		m_pCameraManager->CreateMainCamera(Vector3(-11.5f, 200.85f, -0.45f) ,
			m_aspectRatio, 3.14159f / 4.0f, 0.1f, 2000.f);

		InitTexTriangle();
	}
	void Renderer::Update()
	{
		//OnKeyboardInput();
		m_pLightManager->GetMainLight()->m_lightDir = g_userData.lightDir;
		m_pLightManager->Update();
		m_pShadowManager->Update();
		UpdateCBV();
		SerializeUserData();
	}
	void Renderer::Render()
	{
		RenderTexTriangle();
	}
	void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		for (size_t i = 0; i < m_graphicsPipelineStates.size(); ++i)
		{
			//m_device->DestoryPipelineState(std::move(m_graphicsPipelineStates[i]));
		}
		m_device = nullptr;


		m_graphicsContext.release();
		m_vertexShaders.clear();
		m_pixelShaders.clear();
		m_computeShaders.clear();
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
		auto passParameter = m_pRenderSource->GetCBVPassParameter();

		passParameter->screenSize = m_device->GetScreenSize();
		passParameter->frameIndex = m_device->GetFrameID();
		passParameter->cameraPosWS = m_pCameraManager->GetMainCamera()->GetPosition4();
		passParameter->viewMatrix = m_pCameraManager->GetMainCamera()->GetViewMat();
		passParameter->projMatrix = m_pCameraManager->GetMainCamera()->GetProj();
		passParameter->nearZ = m_pCameraManager->GetMainCamera()->GetNearZ();
		passParameter->farZ = m_pCameraManager->GetMainCamera()->GetFarZ();

		passParameter->mainLight = std::move(m_pLightManager->GetMainLight()->CreateLightData());
		
		passParameter->shadowMatrix = m_pShadowManager->GetMainShadow()->GetShadowMat();
		passParameter->shadowSize = GetScreenSize(Vector2(m_pShadowManager->GetMainShadow()->GetWidth(),
			m_pShadowManager->GetMainShadow()->GetHeight()));
		passParameter->shadowNearZ = m_pShadowManager->GetMainShadow()->GetNearZ();
		passParameter->shadowFarZ = m_pShadowManager->GetMainShadow()->GetFarZ();
		passParameter->shadowDepthBias = g_userData.shadowDepthBias / 100;
		passParameter->shadowSlopeDepthBias = g_userData.shadowSlopeDepthBias / 100;
		passParameter->shadowMaxSlopeDepthBias = g_userData.shadowMaxSlopeDepthBias / 100;

		m_pBufferManager->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(passParameter, sizeof(CBVMainPassParameter));
		
		/*XMStoreFloat4x4(&m_shadowPassParameter.shadowMatrix, XMMatrixTranspose(m_mainLightShadow->GetShadowMat()));
		XMStoreFloat4x4(&m_shadowPassParameter.viewMatrix, XMMatrixTranspose(m_mainLightShadow->GetViewMat()));
		XMStoreFloat4x4(&m_shadowPassParameter.projMatrix, XMMatrixTranspose(m_mainLightShadow->GetProjMat()));
		m_shadowPassParameter.nearZ = m_mainLightShadow->GetNearZ();
		m_shadowPassParameter.farZ = m_mainLightShadow->GetFarZ();
		m_perObjectBindResourceSpace.m_CBVResource[ElysiaHelper::PER_PASS_SPACE][static_cast<size_t>(CBVPassParameterType::Shadow)]->SetMappedData(&m_shadowPassParameter, sizeof(CBVShadowPassParameter));*/

	}
	void Renderer::UpdateObjectCBV()
	{
		for (UINT meshIndex = 0; meshIndex < m_pModelImporter->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = m_pModelImporter->GetMeshRenderer(meshIndex);
			auto objectConstantParameter = *meshRenderer.m_CBVObjectParameter;
			for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
			{
				objectConstantParameter.baseColorTint = g_userData.BaseColorTint;
				objectConstantParameter.opacity = g_userData.Opacity;
				objectConstantParameter.cutoff = g_userData.Cutoff;
				objectConstantParameter.normalIntensity = g_userData.NormalIntensity;
				objectConstantParameter.metallicIntensity = g_userData.MetallicIntensity;
				objectConstantParameter.roughnessIntensity = g_userData.RoughnessIntensity;
				objectConstantParameter.ambientCubemapIntensity = g_userData.AmbientCubemapIntensity;
				objectConstantParameter.ambientCubemapTint = g_userData.AmbientCubemapTint;

				auto objectContantBuffer = m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, frameIndex, meshIndex);
				objectContantBuffer->SetMappedData(&objectConstantParameter, sizeof(CBVObjectParameter));
			}
		}
	}

	void Renderer::InitTexTriangle()
	{
		LoadShaders();

		m_pModelImporter->CreateVertexBuffer();
		m_pModelImporter->CreateIndexBuffer();
		m_pModelImporter->CreateMeshRenders();

 		LoadConstantBuffers();

		LoadTextures(); 

		CreateCreamDepthRT();
		m_pShadowManager->SetMainLight(m_pLightManager->GetMainLight());
		m_pShadowManager->CreateMainShadow(4096, 15);
		m_pRenderSource->GetCBVPassParameter()->ShadowTexIndex = m_pShadowManager->GetMainShadow()->GetShadowRT()->GetResourceHeapIndex();

		CreatePOS();
	}

	void Renderer::LoadShaders()
	{
		AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"PS", ShaderType::Pixel);

		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"PS", ShaderType::Pixel);
		 
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"VS", ShaderType::Vertex);
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"PS", ShaderType::Pixel);
	}
	void Renderer::LoadConstantBuffers()
	{
		m_perMainPassBindResourceSpace	= std::make_unique<PipelineResourceSpace>();
		m_perObjectBindResourceSpace	= std::make_unique<PipelineResourceSpace>();

		BufferCreationDesc desc{};
		desc.m_accessFlags = BufferAccessFlags::HostWritable;
		desc.m_viewFlags = GPUResourceFlags::CBV;
		desc.m_isRawAccess = false;
		
		desc.m_size = sizeof(CBVMainPassParameter);
		m_pBufferManager->AddConstantBuffer(PER_PASS_SPACE, desc);
		m_pBufferManager->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(m_pRenderSource->GetCBVPassParameter(), sizeof(CBVMainPassParameter));
		m_perMainPassBindResourceSpace->SetCBV(m_pBufferManager->GetSingleConstantBuffer(PER_PASS_SPACE));
		m_perMainPassBindResourceSpace->Lock();

		desc.m_size = sizeof(CBVObjectParameter);
		for (UINT meshIndex = 0; meshIndex < m_pModelImporter->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = m_pModelImporter->GetMeshRenderer(meshIndex);

			m_pBufferManager->AddConstantBuffer(PER_OBJECT_SPACE, desc);

			auto objectConstantParameter = *meshRenderer.m_CBVObjectParameter;
			for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
			{
				auto objectContantBuffer = m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, frameIndex, meshIndex);
				objectContantBuffer->SetMappedData(&objectConstantParameter, sizeof(CBVObjectParameter));
			}
		}
		m_perObjectBindResourceSpace->SetCBV(m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, 0, 0));
		m_perObjectBindResourceSpace->Lock();
	}
	void Renderer::CreateCreamDepthRT()
	{
		TexCreateDesc depthBufferCreateDesc{};
		depthBufferCreateDesc.m_name = L"Camera Depth RT";
		depthBufferCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferCreateDesc.m_resouceDesc.Width = static_cast<UINT64>(m_device->GetScreenSize().x);
		depthBufferCreateDesc.m_resouceDesc.Height = static_cast<UINT>(m_device->GetScreenSize().y);
		depthBufferCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

		m_pBufferManager->AddDepthBuffer(std::move(m_device->CreateTexture(depthBufferCreateDesc)));
	}
	void Renderer::LoadTextures()
	{
		TextureCreationDesc texBufferCreateDesc{};

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_E_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			 
			m_pRenderSource->GetCBVPassParameter()->GGX_E_LUT_Index = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));
		}

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_Eavg_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));

			m_pRenderSource->GetCBVPassParameter()->GGX_Eavg_LUT_Index = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));

		}
		  
		{
			texBufferCreateDesc.texturePath = L"Tex\\cubemap0.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			 
			m_pRenderSource->GetCBVPassParameter()->SkyboxTexIndex = newTex->GetResourceHeapIndex();

			m_pTextureManager->AddTextureResource(std::move(newTex));
		}
	}
	void Renderer::CreatePOS()
	{
		PipelineResourceLayout meshResourceLayout{};
		PipelineStateCreateDesc pipelineStateCreateDesc{};
		  
		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = m_perObjectBindResourceSpace.get();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = m_perMainPassBindResourceSpace.get();

		/// Shadow PSO
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Shadow][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Shadow][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 0;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_pShadowManager->GetMainShadow()->GetShadowRT()->GetResourceDesc().Format;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		m_graphicsPipelineStates[ShaderQueue::Shadow] = std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

		/// Opaque PSO
		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = m_perObjectBindResourceSpace.get();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = m_perMainPassBindResourceSpace.get();
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Opaque][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Opaque][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_pBufferManager->GetCameraDepthBuffer()->GetResourceDesc().Format;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		m_graphicsPipelineStates[ShaderQueue::Opaque] = std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

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

	void Renderer::AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType)
	{
		ShaderCreateDesc VSShaderCreateDesc{};
		VSShaderCreateDesc.shaderName = shaderName;
		VSShaderCreateDesc.entryPoint = entryPoint;
		VSShaderCreateDesc.shaderType = shaderType;

		std::unique_ptr<DX12Shader> shader = std::move(m_device->CreateShader(VSShaderCreateDesc));
		std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>> value{};
		value[shaderType] = std::move(shader);

		switch (shaderType)
		{
			case ShaderType::Vertex:
			{
				m_vertexShaders[shaderQueue] = std::move(value);
				break;
			}
			{
			case ShaderType::Pixel:
				m_pixelShaders[shaderQueue] = std::move(value);
			}
		}

	}
	void Renderer::AddUIItems()
	{
		ImGui::ShowDemoWindow();
		if (ImGui::CollapsingHeader("Light"))
		{
			auto mainLight = m_pRenderSource->GetCBVPassParameter();
			ImGui::ColorEdit3("Color", (float*)&g_userData.lightColor);
			ImGui::DragFloat3("Direction", (float*)&g_userData.lightDir, 1, -1, 1);
			ImGui::SliderFloat("Intensity", &g_userData.lightIntensity, 0, 5);

			ImGui::Combo("Shadow Quality", &g_userData.shadowQuality);
			ImGui::SliderFloat("Shadow Depth Bias", &g_userData.shadowDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Slope Depth Bias", &g_userData.shadowSlopeDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Max Slope Depth Bias", &g_userData.shadowMaxSlopeDepthBias, 0, 10);
		}

		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&g_userData.BaseColorTint);
			ImGui::SliderFloat("Opacity", &g_userData.Opacity, 0.f, 1.f);
			ImGui::SliderFloat("Cutoff", &g_userData.Cutoff, 0.f, 1.f);
			ImGui::SliderFloat("Normal Intensity", &g_userData.NormalIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Metallic Intensity", &g_userData.MetallicIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Roughness Intensity", &g_userData.RoughnessIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &g_userData.AmbientCubemapIntensity, 0.f, 2.f);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&g_userData.AmbientCubemapTint);
		}
	}

	void Renderer::RenderTexTriangle() 
	{
		m_device->BeginFrame();
		m_graphicsContext->Reset(m_graphicsPipelineStates[ShaderQueue::Opaque]->m_pipelineState->GetPipelineState());

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		{
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			AddUIItems();
			ImGui::Render();

			m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_graphicsContext->AddBarrier(*m_pBufferManager->GetCameraDepthBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
			m_graphicsContext->FlushBarrier();
			m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_graphicsContext->SetIndexBuffer(m_pBufferManager->GetIndexBufferView());
			m_graphicsContext->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(m_pBufferManager->GetVertexBufferView()));
		}

		DrawShadow();
		DrawOpaque();
		DrawUI();

		{
			m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
			m_graphicsContext->FlushBarrier();

			m_device->SubmitContextWork(*m_graphicsContext);

			m_device->EndFrame();
			m_device->Present();
		}
	} 
	
	void Renderer::DrawShadow()
	{
		auto mainShadow = m_pShadowManager->GetMainShadow();
		auto shadowTexResource = mainShadow->GetShadowRT();

		m_graphicsContext->AddBarrier(*shadowTexResource, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearDepthStencilTarget(*shadowTexResource, 1.f, 0);

		m_graphicsContext->SetViewport(mainShadow->GetViewport());
		m_graphicsContext->SetScissorRect(mainShadow->GetScissorRect());

		// shadow don't need write color so don't set render target
		// only write depth, so need set depthStencil target
		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_graphicsPipelineStates[ShaderQueue::Shadow].get();
		pipelineStateData.m_depthStencilTarget = shadowTexResource;

		bool isReady = true;
		{
			if (shadowTexResource == nullptr)
			{
				ThrowRuntimeError("null tex resource");;
			}
			isReady &= shadowTexResource->GetIsReady();
		}
		if (isReady)
		{
			m_graphicsContext->SetPipeline(pipelineStateData);
			m_graphicsContext->SetPipelineResource(PER_PASS_SPACE, m_perMainPassBindResourceSpace.get());

			UINT vertexStride = m_pModelImporter->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < m_pModelImporter->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = m_pModelImporter->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, m_device->GetFrameID(), meshIndex);
				m_perObjectBindResourceSpace->SetCBV(objectContantBuffer);
				m_graphicsContext->SetPipelineResource(PER_OBJECT_SPACE, m_perObjectBindResourceSpace.get());

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_graphicsContext->Draw(indexCount, startVertex, startIndex);
			}
		}

		m_graphicsContext->AddBarrier(*shadowTexResource, D3D12_RESOURCE_STATE_GENERIC_READ);
		m_graphicsContext->FlushBarrier();
	}
	void Renderer::DrawOpaque()
	{
		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1, 1, 1));
		m_graphicsContext->ClearDepthStencilTarget(*m_pBufferManager->GetCameraDepthBuffer(), 1.f, 0);

		m_graphicsContext->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(m_device->GetScreenSize().x), static_cast<UINT>(m_device->GetScreenSize().y)));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_graphicsPipelineStates[ShaderQueue::Opaque].get();
		pipelineStateData.m_renderTargets.emplace_back(&currBackBuffer);
		pipelineStateData.m_depthStencilTarget = m_pBufferManager->GetCameraDepthBuffer();

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
			m_graphicsContext->SetPipelineResource(PER_PASS_SPACE, m_perMainPassBindResourceSpace.get());

			UINT vertexStride = m_pModelImporter->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < m_pModelImporter->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = m_pModelImporter->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, m_device->GetFrameID(), meshIndex);
				m_perObjectBindResourceSpace->SetCBV(objectContantBuffer);
				m_graphicsContext->SetPipelineResource(PER_OBJECT_SPACE, m_perObjectBindResourceSpace.get());

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