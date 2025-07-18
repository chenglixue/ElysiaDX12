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
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::unique_ptr<DX12VertexBuffer> m_vertexBuffer = nullptr;
		std::vector<std::unique_ptr<DX12RootParameter>> m_rootParameters;
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers;
		std::unique_ptr<DX12RootSignature> m_rootSignature = nullptr;
		std::unique_ptr<DX12Shader> m_vertexShader = nullptr;
		std::unique_ptr<DX12Shader> m_pixelShader = nullptr;
		std::unique_ptr<DX12Shader> m_computeShader = nullptr;
		std::unique_ptr<DX12GraphicsPipelineState> m_graphicsPipelineState = nullptr;
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
		InitTriangle();
	}
	inline void Renderer::Update()
	{

	}
	inline void Renderer::Render()
	{
		RenderTriangle();
	}
	inline void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		m_device->DestoryBuffer(std::move(m_vertexBuffer));
		m_device->DestoryPipelineState(std::move(m_graphicsPipelineState));
		m_device->DestoryShader(std::move(m_vertexShader));
		m_device->DestoryShader(std::move(m_pixelShader));
		m_device = nullptr;

		m_rootParameters.clear();
		m_graphicsContext.release();
		m_vertexBuffer.release();
		m_rootSignature.release();
		m_vertexShader.release();
		m_pixelShader.release();
		m_computeShader.release();
		m_graphicsPipelineState.release();

		if (IDXGIDebug* dxgiDebug = nullptr)
		{
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
			{
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
				dxgiDebug->Release();
			}
		}
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

		BufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
		vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
		vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
		vertexBufferCreationDesc.m_isRawAccess = false;

		m_vertexBuffer = std::move(m_device->CreateVertexBuffer(vertexBufferCreationDesc));
		m_vertexBuffer->SetMappedData(&triangleVertices, sizeof(triangleVertices));

		m_rootSignature = std::move(m_device->CreateRootSignature());

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
		m_vertexShader = std::move(m_device->CreateShader(VSShaderCreateDesc));

		ShaderCreateDesc PSShaderCreateDesc{};
		PSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		PSShaderCreateDesc.entryPoint = "PS";
		PSShaderCreateDesc.shaderType = ShaderType::Pixel;
		m_pixelShader = std::move(m_device->CreateShader(PSShaderCreateDesc));

		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShader.get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShader.get();
		pipelineStateCreateDesc.m_rootSignature = m_rootSignature.get();
		pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		m_graphicsPipelineState = std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc));
	}
	void Renderer::InitTexTriangle()
	{
		struct TriangleVertex
		{
			XMFLOAT3 position;
			XMFLOAT2 uv;
		};

		std::array<TriangleVertex, 3> triangleVertices;
		triangleVertices[0].position = { 0, 0.5, 0.f };
		triangleVertices[0].uv = { 0.5, 0 };
		triangleVertices[1].position = { 0.5f, -0.5f, 0.f };
		triangleVertices[1].uv = { 1, 1 };
		triangleVertices[2].position = { -0.5f, -0.5f, 0.f };
		triangleVertices[2].uv = { 0, 1 };

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Create Vertex Buffer
		{
			BufferCreationDesc vertexBufferCreationDesc{};
			vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
			vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
			vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
			vertexBufferCreationDesc.m_isRawAccess = false;

			m_vertexBuffer = std::move(m_device->CreateVertexBuffer(vertexBufferCreationDesc));
			m_vertexBuffer->SetMappedData(&triangleVertices, sizeof(triangleVertices));
		}

		// Create Root Parameter & Sampler & Root Signature
		{
			{
				auto rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
				rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

				m_rootParameters.push_back(std::move(rootParameter));
			}

			{
				D3D12_SAMPLER_DESC samplerDesc = {};
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.MipLODBias = 0;
				samplerDesc.MaxAnisotropy = 0;
				samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				//samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				samplerDesc.MinLOD = 0;
				samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			}
		}


	}



	void Renderer::RenderClearColor()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0.3, 0.3, 0.8));

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(m_graphicsContext.get());

		m_device->Present();
		m_device->EndFrame();
	}
	void Renderer::RenderTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		m_graphicsContext->Reset(m_graphicsPipelineState->GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0., 0., 0.));

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineState.get(), 
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }));
		m_graphicsContext->SetPipeline(pipelineStateData);
		m_graphicsContext->SetDefaultViewportAndScissor(m_device->GetScreenSize());
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_graphicsContext->Draw(3, 0);

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(m_graphicsContext.get());

		m_device->Present();
		m_device->EndFrame();
	}
	void Renderer::RenderTexTriangle()
	{

	}
}