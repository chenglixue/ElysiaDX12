#pragma once
#include "stdafx.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
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
		struct TriangleVertex
		{
			Vector2 position;
			Vector3 color;
		};
		void RenderClearColor();

		void InitTriangle();
		void RenderTriangle();

		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::unique_ptr<DX12VertexBuffer> m_vertexBuffer = nullptr;
		std::unique_ptr<DX12RootSignature> m_rootSignature = nullptr;
		std::unique_ptr<DX12Shader> m_vertexShader = nullptr;
		std::unique_ptr<DX12Shader> m_pixelShader = nullptr;
		std::unique_ptr<DX12Shader> m_computeShader = nullptr;
		std::unique_ptr<DX12GraphicsPipelineState> m_graphicsPipelineState = nullptr;
	};

	Renderer::Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize)
	{
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
		RenderClearColor();
	}
	inline void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		m_device = nullptr;
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

	void Renderer::InitTriangle()
	{
		std::array<TriangleVertex, 3> triangleVertices;
		triangleVertices[0].position = { -0.5, -0.5 };
		triangleVertices[0].color = { 1, 0, 0 };
		triangleVertices[1].position = { 0.f, 0.5f };
		triangleVertices[1].color = { 0, 1, 0 };
		triangleVertices[2].position = { 0.5f, -0.5f };
		triangleVertices[2].color = { 0, 0, 1 };

		BufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
		vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
		vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
		vertexBufferCreationDesc.m_isRawAccess = false;

		m_vertexBuffer = std::move(m_device->CreateVertexBuffer(vertexBufferCreationDesc, &triangleVertices));

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

		m_vertexShader = std::move(m_device->CreateShader(VSShaderCreateDesc, "VS"));

		ShaderCreateDesc PSShaderCreateDesc{};
		PSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		PSShaderCreateDesc.entryPoint = "PS";
		PSShaderCreateDesc.shaderType = ShaderType::Pixel;
		m_pixelShader = std::move(m_device->CreateShader(PSShaderCreateDesc, "PS"));

		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_vertexShader.get();
		pipelineStateCreateDesc.m_pixelShader = m_pixelShader.get();
		pipelineStateCreateDesc.m_rootSignature = m_rootSignature.get();
		pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;

		m_graphicsPipelineState = std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc));
	}
	void Renderer::RenderTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		m_graphicsContext->Reset(m_graphicsPipelineState->GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0.3, 0.3, 0.8));

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineState.get(), 
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }));
		m_graphicsContext->SetPipeline(pipelineStateData);
		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		m_graphicsContext->SetDefaultViewportAndScissor(m_device->GetScreenSize());

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

	}
}