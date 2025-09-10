#include "Renderer.h"

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
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Init()
	{
		{
			m_mainCamera = InitCamera(XMVectorSet(0.0f, 3.0f, -10.0f, 1.f), m_aspectRatio, 0.8f, 1.f, 1000.f);
			
			m_passParameter.projMatrix = XMMatrixTranspose(m_mainCamera->GetProjMat());
			XMStoreFloat4(&m_passParameter.cameraPosWS, m_mainCamera->GetCameraPos());

			m_cameras.emplace_back(m_mainCamera);
		}

		LoadModel();
		InitLight();
		InitTexTriangle();

		m_objectParameters.reserve(objectNum);
		m_objectParameters = std::vector<CBVObjectParameter>(m_objectParameters.capacity());
	}
	void Renderer::Update()
	{
		OnKeyboardInput();

		m_curRotationAngleRad += m_rotationSpeed;
		m_worldMatrix = XMMatrixRotationY(0);

		m_passParameter.screenSize = m_device->GetScreenSize();
		m_passParameter.frameIndex = m_device->GetFrameID();
		m_passParameter.nearZ = m_mainCamera->GetNearZ();
		m_passParameter.farZ = m_mainCamera->GetFarZ();
		m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_PASS_SPACE][0]->SetMappedData(&m_passParameter, sizeof(CBVPassParameter));
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
			m_device->DestoryPipelineState(std::move(m_graphicsPipelineStates[i]));
		}
		m_device = nullptr;

		m_rootParameters.clear();
		m_graphicsContext.release();
		m_vertexBuffer.release();
		m_rootSignatures.clear();
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
		if ((btnState & MK_LBUTTON) != 0)
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f * m_mainCamera->GetCameraSpeed() * static_cast<float>(x - m_lastMousePos.x));
			float dy = XMConvertToRadians(0.25f * m_mainCamera->GetCameraSpeed() * static_cast<float>(y - m_lastMousePos.y));

			m_mainCamera->Pitch(dy);
			m_mainCamera->Yaw(dx);
		}

		m_lastMousePos.x = x;
		m_lastMousePos.y = y;
	}
	void Renderer::OnKeyboardInput()
	{
		if (GetAsyncKeyState('W') & 0x8000)
			m_mainCamera->MoveVertical(m_mainCamera->GetCameraSpeed() * 0.01);

		if (GetAsyncKeyState('S') & 0x8000)
			m_mainCamera->MoveVertical(-m_mainCamera->GetCameraSpeed() * 0.01);

		if (GetAsyncKeyState('A') & 0x8000)
			m_mainCamera->MoveHorizon(-m_mainCamera->GetCameraSpeed() * 0.01);

		if (GetAsyncKeyState('D') & 0x8000)
			m_mainCamera->MoveHorizon(m_mainCamera->GetCameraSpeed() * 0.01);

		m_mainCamera->UpdateViewMatrix();

		if (m_mainCamera->IsViewDirty())
		{
			XMStoreFloat4(&m_passParameter.cameraPosWS, m_mainCamera->GetCameraPos());
			m_passParameter.viewMatrix = XMMatrixTranspose(m_mainCamera->GetViewMat());
			m_passParameter.projMatrix = XMMatrixTranspose(m_mainCamera->GetProjMat());
			XMStoreFloat4(&m_passParameter.cameraPosWS, m_mainCamera->GetCameraPos());
		}

		m_mainCamera->DisableViewDirty();
	}



	void Renderer::InitTexTriangle()
	{
		LoadShaders();

		LoadVertexIndexBuffer();

		LoadConstantBuffers();

		CreateCreamDepthRT();

		LoadAndCreateTexs();

		CreateSignatures();

		CreatePOS();
	}

	void Renderer::LoadShaders()
	{
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\DrawTriangle.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\DrawTriangle.hlsl", L"PS", ShaderType::Pixel);
		AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Skybox, L"Shaders\\public\\Skybox.hlsl", L"PS", ShaderType::Pixel);
	}
	void Renderer::LoadVertexIndexBuffer()
	{
		AddVertexBuffer(sizeof(DX12Vertex));
		AddIndexBuffer(sizeof(UINT), DXGI_FORMAT_R32_UINT);
	}
	void Renderer::LoadConstantBuffers()
	{
		ConstantBufferCreationDesc constantBufferCreationDesc{};
		constantBufferCreationDesc.m_bufferSize = sizeof(CBVPassParameter);
		constantBufferCreationDesc.m_bufferIndex = 0;
		constantBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		constantBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::CBV;
		constantBufferCreationDesc.m_isRawAccess = false;

		UINT passSpaceIndex = 0;

		auto constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
		constantBuffer->SetMappedData(&m_passParameter, sizeof(CBVPassParameter));
		m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_PASS_SPACE].emplace_back(std::move(constantBuffer));
		m_pipelineBindResource.CBVIndexs[ElysiaHelper::PER_PASS_SPACE] = passSpaceIndex++;

		constantBufferCreationDesc.m_bufferSize = sizeof(CBVObjectParameter);
		m_objectParameters.reserve(objectNum);
		for (int i = 0; i < objectNum; ++i)
		{
			constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE].emplace_back(std::move(constantBuffer));
		}
	}
	void Renderer::CreateCreamDepthRT()
	{
		{
			m_depthBufferCreateDesc.m_name = L"Camera Depth RT";
			m_depthBufferCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D32_FLOAT;
			m_depthBufferCreateDesc.m_resouceDesc.Width = m_device->GetScreenSize().x;
			m_depthBufferCreateDesc.m_resouceDesc.Height = m_device->GetScreenSize().y;
			m_depthBufferCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

			m_depthBuffer = std::move(m_device->CreateTexture(m_depthBufferCreateDesc));
		}
	}
	void Renderer::CreateShadowRT()
	{
		TexCreateDesc shadowCreateDesc{};
		shadowCreateDesc.m_name = L"Shadowm RT";
		shadowCreateDesc.m_resouceDesc.Width = 4096;
		shadowCreateDesc.m_resouceDesc.Height = 4096;
		shadowCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D32_FLOAT;
		shadowCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

		auto shadowTex = std::move(m_device->CreateTexture(m_depthBufferCreateDesc));
		auto shadowMap = std::make_unique<DX12Shadow>(std::move(shadowTex));
		m_shadowBuffers.emplace_back(std::move(shadowMap));
	}
	void Renderer::LoadAndCreateTexs()
	{
		auto texPaths = m_globalTexPaths;
		for (int i = 0; i < texPaths.size(); ++i)
		{
			auto strPath = ElysiaHelper::LPCWSTRToString(texPaths[i]);

			TextureCreationDesc texBufferCreateDesc{};

			texBufferCreateDesc.texturePath = texPaths[i];
			texBufferCreateDesc.isSRGB = false;

			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_PASS_SPACE].emplace_back(std::move(newTex));
		}

		texPaths = m_objectTexPaths;
		for (int i = 0; i < texPaths.size(); ++i)
		{
			auto strPath = ElysiaHelper::LPCWSTRToString(texPaths[i]);
			bool isSRGB = strPath.find("_BaseColor") == strPath.npos ? false : true;

			TextureCreationDesc texBufferCreateDesc{};

			texBufferCreateDesc.texturePath = texPaths[i];
			texBufferCreateDesc.isSRGB = isSRGB;

			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_OBJECT_SPACE].emplace_back(std::move(newTex));
		}
	}
	void Renderer::CreateSignatures()
	{
		{
			auto rootParameter = std::make_unique<DX12RootParameter>();
			rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_PASS_SPACE].size()),
				0, 0, ElysiaHelper::PER_PASS_SPACE);
			m_rootParameters.emplace_back(std::move(rootParameter));

			rootParameter = std::make_unique<DX12RootParameter>();
			rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_OBJECT_SPACE].size()),
				0, 0, ElysiaHelper::PER_OBJECT_SPACE);
			m_rootParameters.emplace_back(std::move(rootParameter));

			rootParameter = std::make_unique<DX12RootParameter>();
			rootParameter->InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_PASS_SPACE);
			m_rootParameters.emplace_back(std::move(rootParameter));

			rootParameter = std::make_unique<DX12RootParameter>();
			rootParameter->InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_OBJECT_SPACE);
			m_rootParameters.emplace_back(std::move(rootParameter));
		}

		RootSignatureCreatDesc rootSignatureCreatDesc{};
		std::vector<DX12RootParameter*> tempRootParameters{};
		tempRootParameters.reserve(m_rootParameters.size());
		for (auto& rootParameter : m_rootParameters)
		{
			tempRootParameters.emplace_back(rootParameter.get());
		}
		rootSignatureCreatDesc.rootParamters = std::move(tempRootParameters);

		m_rootSignatures.emplace_back(std::move(m_device->CreateRootSignature(rootSignatureCreatDesc)));
	}
	void Renderer::CreatePOS()
	{
		/// Opaque PSO
		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Opaque][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Opaque][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_rootSignature = m_rootSignatures.back().get();
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_depthBufferCreateDesc.m_resouceDesc.Format;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		m_graphicsPipelineStates.insert({ ShaderQueue::Opaque, std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)) });

		/// Skybox PSO
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Skybox][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Skybox][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_rootSignature = m_rootSignatures.back().get();
		pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_depthBufferCreateDesc.m_resouceDesc.Format;
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		// skybox not cull back and front
		pipelineStateCreateDesc.m_rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		// let cubemap z = 1 pass z-test, otherwise it'll be failed in z-test because data of zbuffer is 1
		pipelineStateCreateDesc.m_depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		m_graphicsPipelineStates.insert({ ShaderQueue::Skybox, std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)) });
	}

	std::shared_ptr<DX12Camera> Renderer::InitCamera(XMVECTOR position, float aspect, float FOVY, float nearZ, float farZ)
	{
		auto camera = std::make_shared<DX12Camera>();

		camera->SetLens(FOVY, aspect, nearZ, farZ);

		static const FXMVECTOR up{ 0.f, 1.f, 0.f, 0.f };
		static const FXMVECTOR at{ 0.f, 0.f, 0.f, 0.f };
		camera->LookAt(position, up, at);

		return camera;
	}
	void Renderer::InitLight()
	{
		auto dirLight = std::make_unique<DX12DirectionLight>(XMFLOAT3(1, 1, 1), XMFLOAT3(0, -360, 0), 1);
		m_passParameter.mainLights[0] = std::move(dirLight->CreateLightData());

		m_lights.emplace_back(std::move(dirLight));
	}
	void Renderer::LoadModel()
	{
		UINT currDrawVertex = 0;
		UINT currStartVertex = 0;
		UINT currStartIndex = 0;

		for (size_t currModelIndex = 0; currModelIndex < m_modelPaths.size(); ++currModelIndex)
		{
			auto currModel = DX12Model(m_modelPaths[currModelIndex]);
			auto currVertices = std::move(currModel.GetVertices());
			auto currIndices = std::move(currModel.GetIndices());

			m_vertices.insert(m_vertices.end(), currVertices.begin(), currVertices.end());
			m_indices.insert(m_indices.end(), currIndices.begin(), currIndices.end());

			currDrawVertex += static_cast<UINT>(currIndices.size());
			currModel.SetDrawIndexCount(currIndices.size());
			currModel.SetVertexOffset(currStartVertex);
			currModel.SetIndexOffset(currStartIndex);
			m_models.emplace_back(std::move(currModel));

			currStartVertex += static_cast<UINT>(currVertices.size());
			currStartIndex += static_cast<UINT>(currIndices.size());
		}
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
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::None;
		vertexBufferCreationDesc.m_isRawAccess = isRawAccess;

		m_vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
		m_vertexBuffer->SetMappedData(m_vertices.data(), vertexBufferCreationDesc.m_size);
	}
	void Renderer::AddIndexBuffer(UINT singIndexSize, DXGI_FORMAT format, BufferAccessFlags bufferAccessFlag)
	{
		IndexBufferCreateDesc indexBufferCreationDesc{};
		indexBufferCreationDesc.m_bufferSize = static_cast<UINT>(m_indices.size()) * singIndexSize;
		indexBufferCreationDesc.m_format = format;
		indexBufferCreationDesc.m_vertexMappedBuffer = m_vertexBuffer->GetMappedBuffer();
		indexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::None;
		indexBufferCreationDesc.bufferAccessFlags = bufferAccessFlag;

		m_indexBuffer = m_device->CreateIndexBuffer(indexBufferCreationDesc);
		m_indexBuffer->SetMappedData(m_indices.data(), indexBufferCreationDesc.m_bufferSize);
	}




	void Renderer::RenderTexTriangle()
	{
		m_device->BeginFrame();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		AddUIItems();
		ImGui::Render();

		m_objectCBVIndex = 0;
		//DrawShadow();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->AddBarrier(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1, 1, 1));
		m_graphicsContext->ClearDepthStencilTarget(*m_depthBuffer, 1.f, 0);

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		m_graphicsContext->SetIndexBuffer(m_indexBuffer->GetIndexBufferView());

		m_graphicsContext->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(m_device->GetScreenSize().x), static_cast<UINT>(m_device->GetScreenSize().y)));
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DrawOpaque();
		DrawSkybox();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_graphicsContext->GetCommandList());

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->EndFrame();
		m_device->Present();
	}

	void Renderer::AddUIItems()
	{
		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::SliderFloat("Speed", &m_mainCamera->GetCameraSpeed(), 0, 2);
			ImGui::SliderFloat("NearZ", &m_mainCamera->GetNearZ(), 0.001, 1000);
			ImGui::SliderFloat("FarZ", &m_mainCamera->GetFarZ(), 0.001, 1000);
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::ColorEdit3("Color", (float*)&m_passParameter.mainLights[0].m_lightColor);
			ImGui::DragFloat3("Direction", (float*)&m_passParameter.mainLights[0].m_lightDir, 1);

			ImGui::SliderFloat("Intensity", &m_passParameter.mainLights[0].m_intensity, 0, 5);
		}

		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&m_objectParameters[0].baseColorTint);
			ImGui::SliderFloat("Opacity", &m_objectParameters[0].opacity, 0.f, 1.f);
			ImGui::SliderFloat("Normal Intensity", &m_objectParameters[0].normalIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Metallic Intensity", &m_objectParameters[0].metallicIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Roughness Intensity", &m_objectParameters[0].roughnessIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &m_objectParameters[0].ambientCubemapIntensity, 0.f, 2.f);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&m_objectParameters[0].ambientCubemapTint);
		}

		//ImGui::ShowDemoWindow();
	}
	void Renderer::DrawShadow()
	{
		auto shadow = m_shadowBuffers.back().get();
		auto shadowBuffer = m_shadowBuffers.back()->GetShadowRT();
		UINT objectCBVIndex = 0;

		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(*shadowBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->AddBarrier(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(*shadowBuffer, Color(1, 1, 1));
		m_graphicsContext->ClearDepthStencilTarget(*m_depthBuffer, 1.f, 0);

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		m_graphicsContext->SetIndexBuffer(m_indexBuffer->GetIndexBufferView());

		m_graphicsContext->SetViewport(shadow->GetViewport());
		m_graphicsContext->SetScissorRect(shadow->GetScissorRect());
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// set pipeline & bind data
		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Shadow].get(),
			std::vector<DX12TextureResource*>{  },
			shadowBuffer);
		m_graphicsContext->SetPipeline(pipelineStateData);
		m_graphicsContext->SetPipelineResource(m_pipelineBindResource);
		DrawCommand(0);
		DrawCommand(1);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_graphicsContext->GetCommandList());

		m_graphicsContext->AddBarrier(*shadowBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();
	}
	void Renderer::DrawOpaque()
	{
		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		m_objectCBVIndex = 0;

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Opaque].get(),
			std::vector<DX12TextureResource*>{ &currBackBuffer },
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);

		{
			XMMATRIX translateMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixIdentity();
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX MVP = XMMatrixIdentity();

			scaleMatrix = XMMatrixScaling(1.f, 1.f, 1.f);
			rotationMatrix = XMMatrixMultiply(XMMatrixRotationY(XMConvertToRadians(-90)), XMMatrixRotationZ(XMConvertToRadians(90)));
			translateMatrix = XMMatrixTranslation(0.f, 0.f, 0.f);
			MVP = rotationMatrix * scaleMatrix * translateMatrix;
			m_objectParameters[m_objectCBVIndex].worldMatrix = XMMatrixTranspose(MVP);
			SetPipelineResource(m_objectCBVIndex);
			DrawCommand(m_objectCBVIndex++);

			translateMatrix = XMMatrixTranslation(0.f, -5.f, 0.f);
			scaleMatrix = XMMatrixIdentity();
			rotationMatrix = XMMatrixIdentity();
			MVP = translateMatrix * scaleMatrix * rotationMatrix;
			m_objectParameters[m_objectCBVIndex].worldMatrix = XMMatrixTranspose(MVP);
			SetPipelineResource(m_objectCBVIndex);
			DrawCommand(m_objectCBVIndex++);
		}
	}
	void Renderer::DrawSkybox()
	{
		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Skybox].get(),
			std::vector<DX12TextureResource*>{ &currBackBuffer },
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);

		{
			XMMATRIX translateMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixIdentity();
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX MVP = XMMatrixIdentity();

			scaleMatrix = XMMatrixScaling(10.f, 10.f, 10.f);
			rotationMatrix = XMMatrixIdentity();
			translateMatrix = XMMatrixTranslation(0.f, 0.0f, 0.0f);
			MVP = ElysiaHelper::GetMVP(translateMatrix, rotationMatrix, scaleMatrix);
			m_objectParameters[m_objectCBVIndex].worldMatrix = XMMatrixTranspose(MVP);
			SetPipelineResource(m_objectCBVIndex);
			DrawCommand(m_objectCBVIndex++);
		}
	}
	void Renderer::SetPipelineResource(UINT objectCBVIndex)
	{
		// get object constant buffer
		auto objectConstanBuffer = m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE][static_cast<size_t>(objectCBVIndex)].get();

		//// set object constant data in buffer
		objectConstanBuffer->SetMappedData(&m_objectParameters[objectCBVIndex], sizeof(CBVObjectParameter));
		m_pipelineBindResource.CBVIndexs.erase(ElysiaHelper::PER_OBJECT_SPACE);
		m_pipelineBindResource.CBVIndexs[ElysiaHelper::PER_OBJECT_SPACE] = objectCBVIndex;
		m_graphicsContext->SetPipelineResource(m_pipelineBindResource);
	}
	void Renderer::DrawCommand(size_t drawModelIndex)
	{
		assert(drawModelIndex < m_models.size());
		auto drawModel = m_models[drawModelIndex];

		auto drawVertexCount = drawModel.GetIndexCount();
		auto startVertex = drawModel.GetVertexOffset();
		auto startIndex = drawModel.GetIndexOffset();

		m_graphicsContext->Draw(drawVertexCount, startVertex, startIndex);
	}
	void Renderer::BindObject(DX12TextureResource& currBackBuffer,
		UINT& objectCBVIndex, uint8_t pipelineStateQueueIndex, size_t drawModelIndex)
	{
		// get object constant buffer
		auto objectConstanBuffer = m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE][static_cast<size_t>(objectCBVIndex)].get();

		//// set object constant data in buffer
		objectConstanBuffer->SetMappedData(&m_objectParameters[objectCBVIndex], sizeof(CBVObjectParameter));
		m_pipelineBindResource.CBVIndexs.erase(ElysiaHelper::PER_OBJECT_SPACE);
		m_pipelineBindResource.CBVIndexs[ElysiaHelper::PER_OBJECT_SPACE] = objectCBVIndex;

		// set pipeline & bind data
		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[pipelineStateQueueIndex].get(),
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }),
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);
		m_graphicsContext->SetPipelineResource(m_pipelineBindResource);

		assert(drawModelIndex < m_models.size());
		auto drawModel = m_models[drawModelIndex];

		auto drawVertexCount = drawModel.GetIndexCount();
		auto startVertex = drawModel.GetVertexOffset();
		auto startIndex = drawModel.GetIndexOffset();

		m_graphicsContext->Draw(drawVertexCount, startVertex, startIndex);
		objectCBVIndex++;
	}
}