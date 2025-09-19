#include "Renderer.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 616; }
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

		m_cameraManager = std::make_unique<CameraManager>();
		m_lightManager = std::make_unique<LightManager>();
		m_shadowManager = std::make_unique<ShadowManager>(m_device.get());
		m_bufferManager = std::make_unique<BufferManager>();
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Init()
	{
		m_cameraManager->Init();
		m_lightManager->Init();
		m_shadowManager->Init();
		m_bufferManager->Init();

		m_shadowManager->CreateMainShadow(4096, 15);

		LoadModel();
		InitTexTriangle();
	}
	void Renderer::Update()
	{
		//OnKeyboardInput();
		UpdateCBV();
	}
	void Renderer::Render()
	{
		RenderTexTriangle();
	}
	void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		m_device->DestoryBuffer(std::move(m_vertexBuffer));
		for (size_t i = 0; i < m_graphicsPipelineStates.size(); ++i)
		{
			//m_device->DestoryPipelineState(std::move(m_graphicsPipelineStates[i]));
		}
		m_device = nullptr;

		m_graphicsContext.release();
		m_vertexBuffer.release();
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
		ZeroMemory(&m_mainPassParameter, sizeof(CBVMainPassParameter));

		m_mainPassParameter.screenSize = m_device->GetScreenSize();
		m_mainPassParameter.frameIndex = m_device->GetFrameID();
		m_mainPassParameter.cameraPosWS = m_cameraManager->GetMainCamera()->GetPosition4();
		m_mainPassParameter.viewMatrix = m_cameraManager->GetMainCamera()->GetViewMat();
		m_mainPassParameter.projMatrix = m_cameraManager->GetMainCamera()->GetProj();
		m_mainPassParameter.nearZ = m_cameraManager->GetMainCamera()->GetNearZ();
		m_mainPassParameter.farZ = m_cameraManager->GetMainCamera()->GetFarZ();
		m_mainPassParameter.mainLight = std::move(m_lightManager->GetMainLight()->CreateLightData());
		m_perMainPassBindResourceSpace->GetCBV()->SetMappedData(&m_mainPassParameter, sizeof(CBVMainPassParameter));
		
		/*XMStoreFloat4x4(&m_shadowPassParameter.shadowMatrix, XMMatrixTranspose(m_mainLightShadow->GetShadowMat()));
		XMStoreFloat4x4(&m_shadowPassParameter.viewMatrix, XMMatrixTranspose(m_mainLightShadow->GetViewMat()));
		XMStoreFloat4x4(&m_shadowPassParameter.projMatrix, XMMatrixTranspose(m_mainLightShadow->GetProjMat()));
		m_shadowPassParameter.nearZ = m_mainLightShadow->GetNearZ();
		m_shadowPassParameter.farZ = m_mainLightShadow->GetFarZ();
		m_perObjectBindResourceSpace.m_CBVResource[ElysiaHelper::PER_PASS_SPACE][static_cast<size_t>(CBVPassParameterType::Shadow)]->SetMappedData(&m_shadowPassParameter, sizeof(CBVShadowPassParameter));*/

	}
	void Renderer::UpdateObjectCBV()
	{
		
		/*CBVObjectParameter objectParameter{};
		XMMATRIX translateMatrix = XMMatrixIdentity();
		XMMATRIX scaleMatrix = XMMatrixIdentity();
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		XMMATRIX MVP = XMMatrixIdentity();

		scaleMatrix = XMMatrixScaling(1.f, 1.f, 1.f);
		rotationMatrix = XMMatrixMultiply(XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixRotationZ(XMConvertToRadians(90)));
		translateMatrix = XMMatrixTranslation(0.f, 0.f, 0.f);
		MVP = rotationMatrix * scaleMatrix * translateMatrix;
		objectParameter.worldMatrix = XMMatrixTranspose(scaleMatrix);
		m_objectConstanBuffers[m_device->GetFrameID()]->SetMappedData(&objectParameter, sizeof(CBVObjectParameter));*/
	}

	void Renderer::InitTexTriangle()
	{
		LoadShaders();

		LoadVertexIndexBuffer();

		LoadConstantBuffers();

		//LoadAndCreateTexs();

		CreateCreamDepthRT();
		//CreateShadowRT();

		CreatePOS();
	}

	void Renderer::LoadShaders()
	{ 
		//AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"VS", ShaderType::Vertex);
		//AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"PS", ShaderType::Pixel);

		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\PBR.hlsl", L"PS", ShaderType::Pixel);
		 
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"VS", ShaderType::Vertex);
		//AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"PS", ShaderType::Pixel);
	}
	void Renderer::LoadVertexIndexBuffer()
	{
		AddVertexBuffer(sizeof(DX12Vertex));
		//AddIndexBuffer(sizeof(UINT), DXGI_FORMAT_R32_UINT);
	}
	void Renderer::LoadConstantBuffers()
	{
		m_perMainPassBindResourceSpace	= std::make_unique<PipelineResourceSpace>();
		m_perObjectBindResourceSpace	= std::make_unique<PipelineResourceSpace>();

		BufferCreationDesc desc{};
		desc.m_size = sizeof(CBVMainPassParameter);
		desc.m_accessFlags = BufferAccessFlags::HostWritable;
		desc.m_viewFlags = GPUResourceFlags::CBV;
		desc.m_isRawAccess = false;
		
		auto pTempConstanBuffer = m_device->CreateBuffer(desc);
		m_bufferManager->AddConstantBuffer(PER_PASS_SPACE, std::move(pTempConstanBuffer));
		m_perMainPassBindResourceSpace->SetCBV(m_bufferManager->GetConstantBuffer(PER_PASS_SPACE));
		m_perMainPassBindResourceSpace->Lock();

		desc.m_size = sizeof(CBVObjectParameter);
		for (int i = 0; i < m_objectConstanBuffers.size(); ++i)
		{
			m_objectConstanBuffers[i] = std::unique_ptr<DX12ConstantBuffer>(m_device->CreateConstantBuffer(constantBufferCreationDesc));
		}
		m_perObjectBindResourceSpace->SetCBV(m_objectConstanBuffers[m_device->GetFrameID()].get());
		m_perObjectBindResourceSpace->Lock();
	}
	void Renderer::CreateCreamDepthRT()
	{
		auto& depthBufferCreateDesc = m_depthBufferCreateDesc["Camera"];
		depthBufferCreateDesc.m_name = L"Camera Depth RT";
		depthBufferCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthBufferCreateDesc.m_resouceDesc.Width = static_cast<UINT64>(m_device->GetScreenSize().x);
		depthBufferCreateDesc.m_resouceDesc.Height = static_cast<UINT>(m_device->GetScreenSize().y);
		depthBufferCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

		m_depthBuffer = std::move(m_device->CreateTexture(depthBufferCreateDesc));
	}
	void Renderer::LoadAndCreateTexs()
	{
		auto texLoadSettings = m_globalTexLoadSettings;
		for (int i = 0; i < texLoadSettings.size(); ++i)
		{
			auto strPath = ElysiaHelper::LPCWSTRToString(texLoadSettings[i].LoadPath);

			TextureCreationDesc texBufferCreateDesc{};

			texBufferCreateDesc.texturePath = texLoadSettings[i].LoadPath;
			texBufferCreateDesc.isSRGB = texLoadSettings[i].IsSRGB;

			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			//m_texs.emplace_back(std::move(m_device->CreateTextureFromFile(texBufferCreateDesc)));
		}

		texLoadSettings = m_objectTexLoadSettings;
		for (int i = 0; i < texLoadSettings.size(); ++i)
		{
			auto strPath = ElysiaHelper::LPCWSTRToString(texLoadSettings[i].LoadPath);

			TextureCreationDesc texBufferCreateDesc{};

			texBufferCreateDesc.texturePath = texLoadSettings[i].LoadPath;
			texBufferCreateDesc.isSRGB = texLoadSettings[i].IsSRGB;

			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			//m_texs.emplace_back(std::move(m_device->CreateTextureFromFile(texBufferCreateDesc)));
		}
	}
	void Renderer::CreatePOS()
	{
		PipelineResourceLayout meshResourceLayout{};
		PipelineStateCreateDesc pipelineStateCreateDesc{};

		/// Shadow PSO
		/*meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = m_perObjectBindResourceSpace;
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = m_perMainPassBindResourceSpace;
		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Shadow][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Shadow][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 0;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_depthBufferCreateDesc["Shadow"].m_resouceDesc.Format;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		m_graphicsPipelineStates.insert({ ShaderQueue::Shadow, std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout)) });*/


		/// Opaque PSO
		meshResourceLayout.m_spaces[PER_OBJECT_SPACE]	= m_perObjectBindResourceSpace.get();
		meshResourceLayout.m_spaces[PER_PASS_SPACE]		= m_perMainPassBindResourceSpace.get();
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Opaque][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Opaque][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_depthBufferCreateDesc["Camera"].m_resouceDesc.Format;
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

	void Renderer::LoadModel()
	{
		UINT currDrawVertex = 0;
		UINT currStartVertex = 0;
		UINT currStartIndex = 0;

		std::vector<DX12Vertex> meshVertices = {
		{{1.0f, -1.0f, 1.0f}, {0.f, 0.f, 0.f}, { 1.0f, 1.0f }, {0.0f, -1.0f, 0.0f}, {0.f, 0.f, 0.f}},
		{{1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
		{{1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{-1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{1.0f, -1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, 1.0f, 1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f, -1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{-1.0f, 1.0f, -1.0f},{0.f, 0.f, 0.f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		};

		m_vertices.insert(m_vertices.begin(), meshVertices.begin(), meshVertices.end());
		//m_indices.insert(m_indices.end(), currIndices.begin(), currIndices.end());

		/*for (size_t currModelIndex = 0; currModelIndex < m_modelPaths.size(); ++currModelIndex)
		{
			auto currModel = DX12Model(m_modelPaths[currModelIndex]);
			auto currVertices = std::move(currModel.GetVertices());
			auto currIndices = std::move(currModel.GetIndices());

			m_vertices.insert(m_vertices.end(), currVertices.begin(), currVertices.end());
			m_indices.insert(m_indices.end(), currIndices.begin(), currIndices.end());

			currDrawVertex += static_cast<UINT>(currIndices.size());
			currModel.SetDrawIndexCount(static_cast<UINT>(currIndices.size()));
			currModel.SetVertexOffset(currStartVertex);
			currModel.SetIndexOffset(currStartIndex);
			m_models.emplace_back(std::move(currModel));

			currStartVertex += static_cast<UINT>(currVertices.size());
			currStartIndex += static_cast<UINT>(currIndices.size());
		}*/
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
	void Renderer::AddVertexBuffer(UINT singVertexSize, BufferAccessFlags bufferAccessFlag, bool isRawAccess)
	{
		VertexBufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = singVertexSize;
		vertexBufferCreationDesc.m_size = static_cast<UINT>(m_vertices.size()) * vertexBufferCreationDesc.m_stride;
		vertexBufferCreationDesc.bufferAccessFlags = bufferAccessFlag;
		vertexBufferCreationDesc.bufferTypeFlags = GPUResourceFlags::None;
		vertexBufferCreationDesc.m_isRawAccess = isRawAccess;

		m_vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
		m_vertexBuffer->SetMappedData(m_vertices.data(), vertexBufferCreationDesc.m_size);
	}

	void Renderer::RenderTexTriangle()
	{
		m_device->BeginFrame();
		m_graphicsContext->Reset();

		/*ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		AddUIItems();
		ImGui::Render();*/

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->AddBarrier(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1, 1, 1));
		m_graphicsContext->ClearDepthStencilTarget(*m_depthBuffer, 1.f, 0);

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_graphicsPipelineStates[ShaderQueue::Opaque].get();
		pipelineStateData.m_renderTargets.emplace_back(&currBackBuffer);
		pipelineStateData.m_depthStencilTarget = m_depthBuffer.get();

		bool isReady = true;
		if (isReady)
		{
			auto& objectParameter = m_objectPassParameters[m_device->GetFrameID()];
			Matrix translateMatrix = Matrix::Identity;
			Matrix scaleMatrix = Matrix::Identity;
			Matrix rotationMatrix = Matrix::Identity;
			Matrix MVP = Matrix::Identity;

			scaleMatrix = Matrix::CreateScale(1);
			rotationMatrix = Matrix::CreateRotationY(-90) * Matrix::CreateRotationZ(90);
			MVP = rotationMatrix * scaleMatrix * translateMatrix;
			objectParameter.worldMatrix = MVP;
			m_objectConstanBuffers[m_device->GetFrameID()]->SetMappedData(&objectParameter, sizeof(CBVObjectParameter));

			m_perObjectBindResourceSpace->SetCBV(m_objectConstanBuffers[m_device->GetFrameID()].get());

			m_graphicsContext->SetPipeline(pipelineStateData);
			m_graphicsContext->SetPipelineResource(PER_OBJECT_SPACE, m_perObjectBindResourceSpace.get());
			m_graphicsContext->SetPipelineResource(PER_PASS_SPACE, m_perMainPassBindResourceSpace.get());

			m_graphicsContext->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(m_device->GetScreenSize().x), static_cast<UINT>(m_device->GetScreenSize().y)));
			m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			DrawCommand(0);
		}
		//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_graphicsContext->GetCommandList());

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->EndFrame();
		m_device->Present();
	}

	void Renderer::AddUIItems()
	{
		/*if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::SliderFloat("Speed", &m_mainCamera->GetCameraSpeed(), 0, 2);
			ImGui::SliderFloat("NearZ", &m_mainCamera->GetNearZ(), 0.001f, 1000);
			ImGui::SliderFloat("FarZ", &m_mainCamera->GetFarZ(), 0.001f, 1000);
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::ColorEdit3("Color", (float*)&m_mainPassParameter.mainLights[0].m_lightColor);
			ImGui::DragFloat3("Direction", (float*)&m_mainPassParameter.mainLights[0].m_lightDir, 1);

			ImGui::SliderFloat("Intensity", &m_mainPassParameter.mainLights[0].m_intensity, 0, 5);
		}

		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&m_objectPassParameters[m_device->GetFrameID()].baseColorTint);
			ImGui::SliderFloat("Opacity", &m_objectPassParameters[m_device->GetFrameID()].opacity, 0.f, 1.f);
			ImGui::SliderFloat("Normal Intensity", &m_objectPassParameters[m_device->GetFrameID()].normalIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Metallic Intensity", &m_objectPassParameters[m_device->GetFrameID()].metallicIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Roughness Intensity", &m_objectPassParameters[m_device->GetFrameID()].roughnessIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &m_objectPassParameters[m_device->GetFrameID()].ambientCubemapIntensity, 0.f, 2.f);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&m_objectPassParameters[m_device->GetFrameID()].ambientCubemapTint);
		}*/

		//ImGui::ShowDemoWindow();
	}
	void Renderer::DrawShadow()
	{
		//auto shadow = m_shadowBuffers.back().get();
		//auto shadowBuffer = m_shadowBuffers.back()->GetShadowRT();
		//m_objectCBVIndex = 0;

		//m_graphicsContext->AddBarrier(*shadowBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		//m_graphicsContext->FlushBarrier();

		//m_graphicsContext->ClearDepthStencilTarget(*shadowBuffer, 1.f, 0);

		//m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		//m_graphicsContext->SetIndexBuffer(m_indexBuffer->GetIndexBufferView());

		//m_graphicsContext->SetViewport(shadow->GetViewport());
		//m_graphicsContext->SetScissorRect(shadow->GetScissorRect());
		//m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//// set pipeline & bind data
		//auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Shadow]->m_pipelineState.get(),
		//	{},
		//	shadowBuffer);
		//m_graphicsContext->SetPipeline(pipelineStateData);
		////SetPipelineResource(i, CBVPassParameterType::Shadow);
		////DrawCommand(i);

		//m_graphicsContext->AddBarrier(*shadowBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
		//m_graphicsContext->FlushBarrier();
	}
	void Renderer::DrawOpaque()
	{
		//auto& currBackBuffer = m_device->GetCurrBackBuffer();

		//PipelineInfo pipelineStateData{};
		//pipelineStateData.m_pipelineStateObject = m_graphicsPipelineStates[ShaderQueue::Opaque].get();
		//pipelineStateData.m_renderTargets.emplace_back(currBackBuffer);
		//pipelineStateData.m_depthStencilTarget = m_depthBuffer.get();

		//bool isReady = true;
		///*for (auto& tex : m_texs)
		//{
		//	isReady = tex->GetIsReady();
		//}*/
		//if(isReady)
		//{
		//	m_perObjectBindResourceSpace->SetCBV(m_objectConstanBuffers[m_device->GetFrameID()].get());

		//	m_graphicsContext->SetPipeline(pipelineStateData);
		//	m_graphicsContext->SetPipelineResource(PER_OBJECT_SPACE, m_perObjectBindResourceSpace.get());
		//	m_graphicsContext->SetPipelineResource(PER_PASS_SPACE, m_perMainPassBindResourceSpace.get());

		//	DrawCommand(0);
		//}
	}
	void Renderer::DrawSkybox()
	{
		/*auto& currBackBuffer = m_device->GetCurrBackBuffer();

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Skybox].get(),
			std::vector<DX12TextureResource*>{ &currBackBuffer },
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);

		{
			SetPipelineResource(m_objectCBVIndex, CBVPassParameterType::Main);
			DrawCommand(m_objectCBVIndex++);
		}*/
	}
	void Renderer::DrawCommand(size_t drawModelIndex)
	{
		/*assert(drawModelIndex < m_models.size());
		auto drawModel = m_models[drawModelIndex];

		auto drawVertexCount = drawModel.GetIndexCount();
		auto startVertex = drawModel.GetVertexOffset();
		auto startIndex = drawModel.GetIndexOffset();*/

		m_graphicsContext->Draw(static_cast<UINT>(m_vertices.size()));
	}
}