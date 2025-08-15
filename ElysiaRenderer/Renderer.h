#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include <dxgidebug.h>

namespace ElysiaRenderer
{
	using namespace DirectX;

	class Renderer
	{
	public:
		Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize);
		~Renderer();

		void Init();
		void Update();
		void Render();
		void Destory();

	private:
		
		void InitTriangle();
		void InitTexTriangle();

		void RenderClearColor();
		void RenderTriangle();
		void RenderTexTriangle();

		float m_aspectRatio;
		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12TextureResource> m_depthBuffer = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::vector<std::unique_ptr<DX12VertexBuffer>> m_vertexBuffers;
		std::vector<std::unique_ptr<DX12RootParameter>> m_rootParameters;
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers;
		std::vector<std::unique_ptr<DX12RootSignature>> m_rootSignatures;
		std::vector<std::unique_ptr<DX12Shader>> m_vertexShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_pixelShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_computeShaders;
		std::vector<std::unique_ptr<DX12GraphicsPipelineState>> m_graphicsPipelineStates;
		PipelineBindResource m_pipelineBindResource;

		struct CBVPassParameter
		{
			XMFLOAT4 cameraPosWS;
			XMFLOAT4X4 viewMatrix;
			XMFLOAT4X4 projMatrix;
			XMFLOAT4 padding[7];
		};
		struct CBVObjectParameter
		{
			XMFLOAT4X4 worldMatrix;
			XMFLOAT4 padding[12];
		};
		static_assert((sizeof(CBVPassParameter) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
		static_assert((sizeof(CBVObjectParameter) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
		CBVPassParameter m_passParameter;
		CBVObjectParameter m_objectParameter;

		XMVECTORF32 m_cameraPos;
		XMMATRIX m_worldMatrix;
		XMMATRIX m_viewMatrix;
		XMMATRIX m_projMatrix;
		float m_nearZ = 0.01f;
		float m_farZ = 1000.f;
		float m_curRotationAngleRad = 0.f;
		const float m_rotationSpeed = 0.01f;
	};

	Renderer::Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize)
	{
		m_aspectRatio = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);

		m_device = std::make_unique<DX12Device>(windowHandle, screenSize);
		m_graphicsContext = m_device->CreateGraphicsContext();

	}

	Renderer::~Renderer()
	{

	}

	inline void Renderer::Init()
	{
		m_passParameter = CBVPassParameter();

		{
			m_worldMatrix = XMMatrixIdentity();

			m_cameraPos = { -3.0f, 3.0f, -8.0f, 0.f };
			static const XMVECTORF32 up{ 0.f, 1.f, 0.f, 0.f };
			static const XMVECTORF32 at{ 0.f, 0.f, 0.f, 0.f };
			m_viewMatrix = XMMatrixLookAtLH(m_cameraPos, at, up);

			m_projMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, m_aspectRatio, m_nearZ, m_farZ);

			XMStoreFloat4x4(&m_passParameter.viewMatrix, (m_viewMatrix));
			XMStoreFloat4x4(&m_passParameter.projMatrix, (m_projMatrix));
			XMStoreFloat4(&m_passParameter.cameraPosWS, m_cameraPos);
		}

		InitTexTriangle();
	}
	inline void Renderer::Update()
	{
		m_curRotationAngleRad += m_rotationSpeed;
		m_worldMatrix = XMMatrixRotationY(m_curRotationAngleRad);
		XMStoreFloat4x4(&m_objectParameter.worldMatrix, m_worldMatrix);
	}
	inline void Renderer::Render()
	{
		RenderTexTriangle();
	}
	inline void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		for (size_t i = 0; i < m_vertexBuffers.size(); ++i)
		{
			m_device->DestoryBuffer(std::move(m_vertexBuffers[i]));
		}
		for (size_t i = 0; i < m_graphicsPipelineStates.size(); ++i)
		{
			m_device->DestoryPipelineState(std::move(m_graphicsPipelineStates[i]));
		}
		for (size_t i = 0; i < m_vertexShaders.size(); ++i)
		{
			m_device->DestoryShader(std::move(m_vertexShaders[i]));
		}
		for (size_t i = 0; i < m_pixelShaders.size(); ++i)
		{
			m_device->DestoryShader(std::move(m_pixelShaders[i]));
		}
		m_device = nullptr;

		m_rootParameters.clear();
		m_graphicsContext.release();
		m_vertexBuffers.clear();
		m_rootSignatures.clear();
		m_vertexShaders.clear();
		m_pixelShaders.clear();
		m_computeShaders.clear();
		m_graphicsPipelineStates.clear();
	}

	void Renderer::InitTriangle()
	{
		struct TriangleVertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		std::array<TriangleVertex, 3> triangleVertices;
		triangleVertices[0].position = { 0, 0.5, 0.f };
		triangleVertices[0].color = { 1, 0, 0, 1.f };
		triangleVertices[1].position = { 0.5f, -0.5f, 0.f };
		triangleVertices[1].color = { 0, 1, 0, 1.f };
		triangleVertices[2].position = { -0.5f, -0.5f, 0.f };
		triangleVertices[2].color = { 0, 0, 1, 1.f };

		VertexBufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
		vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
		vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
		vertexBufferCreationDesc.m_isRawAccess = false;

		auto vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
		vertexBuffer->SetMappedData(triangleVertices.data(), sizeof(triangleVertices));
		m_vertexBuffers.push_back(std::move(vertexBuffer));

		//m_rootSignature = std::move(m_device->CreateRootSignature());

		// Define the vertex input layout.
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		ShaderCreateDesc VSShaderCreateDesc{};
		VSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		VSShaderCreateDesc.entryPoint = "VS";
		VSShaderCreateDesc.shaderType = ShaderType::Vertex;
		m_vertexShaders.push_back(std::move(m_device->CreateShader(VSShaderCreateDesc)));

		ShaderCreateDesc PSShaderCreateDesc{};
		PSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		PSShaderCreateDesc.entryPoint = "PS";
		PSShaderCreateDesc.shaderType = ShaderType::Pixel;
		m_pixelShaders.push_back(std::move(m_device->CreateShader(PSShaderCreateDesc)));

		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShaders.back().get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShaders.back().get();
		pipelineStateCreateDesc.m_rootSignature = m_rootSignatures.back().get();
		pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		m_graphicsPipelineStates.push_back(std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)));
	}
	void Renderer::InitTexTriangle()
	{
		struct TexTriangleVertex
		{
			XMFLOAT3 position;

			/*XMFLOAT2 uv;

			XMFLOAT3 normal;*/

			XMFLOAT4 color;
		};

		std::random_device seed;
		std::ranlux48 engine(seed());
		std::uniform_real_distribution<> distrib(0.f, 1.f);
		XMFLOAT3 randomColors[36];
		for (int i = 0; i < 36; ++i)
		{
			randomColors[i] = { (float)distrib(engine), (float)distrib(engine), (float)distrib(engine) };
		}
		int colorIndex = 0;
		TexTriangleVertex triangleVertices[] = 
		{
			{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
		};

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			/*{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, 
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },*/
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Create Shader
		{
			ShaderCreateDesc VSShaderCreateDesc{};
			VSShaderCreateDesc.shaderName = L"Shaders\\DrawTriangle.hlsl";
			VSShaderCreateDesc.entryPoint = "VS";
			VSShaderCreateDesc.shaderType = ShaderType::Vertex;
			m_vertexShaders.push_back(std::move(m_device->CreateShader(VSShaderCreateDesc)));

			ShaderCreateDesc PSShaderCreateDesc{};
			PSShaderCreateDesc.shaderName = L"Shaders\\DrawTriangle.hlsl";
			PSShaderCreateDesc.entryPoint = "PS";
			PSShaderCreateDesc.shaderType = ShaderType::Pixel;
			m_pixelShaders.push_back(std::move(m_device->CreateShader(PSShaderCreateDesc)));
		}

		// Create Vertex Buffer
		{
			VertexBufferCreationDesc vertexBufferCreationDesc{};
			vertexBufferCreationDesc.m_stride = sizeof(TexTriangleVertex);
			vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
			vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
			vertexBufferCreationDesc.m_isRawAccess = false;

			auto vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
			vertexBuffer->SetMappedData(&triangleVertices, sizeof(triangleVertices));
			m_vertexBuffers.push_back(std::move(vertexBuffer));
		}

		// Create Constant Buffer
		{
			ConstantBufferCreationDesc constantBufferCreationDesc{};
			constantBufferCreationDesc.m_size = sizeof(CBVPassParameter);
			constantBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			constantBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::CBV;
			constantBufferCreationDesc.m_isRawAccess = false;

			auto constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			constantBuffer->SetMappedData(&m_passParameter, sizeof(CBVPassParameter));
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_PASS_SPACE] = std::move(constantBuffer);

			constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			constantBuffer->SetMappedData(&m_objectParameter, sizeof(CBVObjectParameter));
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE] = std::move(constantBuffer);
		}

		// Create Depth Buffer
		TexCreateDesc depthBufferCreateDesc{};
		{
			depthBufferCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthBufferCreateDesc.m_resouceDesc.Width = m_device->GetScreenSize().x;
			depthBufferCreateDesc.m_resouceDesc.Height = m_device->GetScreenSize().y;
			depthBufferCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

			m_depthBuffer = std::move(m_device->CreateTexture(depthBufferCreateDesc));
		}

		// Create Tex & Buffer
		{
			TextureCreationDesc texBufferCreateDesc{};

			texBufferCreateDesc.texturePath = L"Tex\\Wood.dds";
			texBufferCreateDesc.isSRGB = true;

			auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
			m_pipelineBindResource.m_SRVResources.push_back(std::move(newTex));
		}

		// Create Root Parameter & Sampler & Root Signature
		{
			{
				auto rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
				rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_pipelineBindResource.m_SRVResources.size(), 
					0, 0, ElysiaHelper::PER_OBJECT_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));

				rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_PASS_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));

				rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_OBJECT_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));
			}

			RootSignatureCreatDesc rootSignatureCreatDesc;
			std::vector<DX12RootParameter*> tempRootParameters{};
			for (size_t i = 0; i < m_rootParameters.size(); ++i)
			{
				tempRootParameters.push_back(m_rootParameters[i].get());
			}
			rootSignatureCreatDesc.rootParamters = std::move(tempRootParameters);

			m_rootSignatures.push_back(std::move(m_device->CreateRootSignature(rootSignatureCreatDesc)));
		}

		// Create PSO
		{
			PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = m_vertexShaders.back().get();
			pipelineStateCreateDesc.m_pixelShader = m_pixelShaders.back().get();
			pipelineStateCreateDesc.m_rootSignature = m_rootSignatures.back().get();
			pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
			pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = depthBufferCreateDesc.m_resouceDesc.Format;
			pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

			

			m_graphicsPipelineStates.push_back(std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)));
		}
	}

	void Renderer::RenderClearColor()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0.3f, 0.3f, 0.8f));

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->Present();
		m_device->EndFrame();
	}
	/*void Renderer::RenderTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		size_t pipelineStateIndex = 0;
		size_t vertexBufferIndex = 0;
		m_graphicsContext->Reset(m_graphicsPipelineStates[pipelineStateIndex]->GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1., 0., 0.));

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffers[vertexBufferIndex]->GetVertexBufferView());
		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[pipelineStateIndex].get(),
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }));
		m_graphicsContext->SetPipeline(pipelineStateData, m_pipelineBindResource);
		m_graphicsContext->SetDefaultViewportAndScissor(m_device->GetScreenSize());
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_graphicsContext->Draw(3, 0);

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->Present();
		m_device->EndFrame();
	}*/
	void Renderer::RenderTexTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		size_t pipelineStateIndex = 0;
		size_t vertexBufferIndex = 0;

		m_graphicsContext->Reset(m_graphicsPipelineStates[pipelineStateIndex]->GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->AddBarrier(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0., 0., 0.));
		m_graphicsContext->ClearDepthStencilTarget(*m_depthBuffer, 1.f, 0);

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffers[vertexBufferIndex]->GetVertexBufferView());
		auto objectConstanBuffer = dynamic_cast <DX12ConstantBuffer*>(m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE].get());
		objectConstanBuffer->SetMappedData(&m_objectParameter, sizeof(CBVObjectParameter));

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[pipelineStateIndex].get(),
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }),
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData, m_pipelineBindResource);
		m_graphicsContext->SetDefaultViewportAndScissor(m_device->GetScreenSize());
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_graphicsContext->Draw(36, 0);

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->EndFrame();
		m_device->Present();
	}
}