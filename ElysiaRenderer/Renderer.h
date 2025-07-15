#pragma once
#include "stdafx.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	class Renderer
	{
	public:
		Renderer(HWND windowHandle, UINT2 screenSize);
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
	};

	Renderer::Renderer(HWND windowHandle, UINT2 screenSize)
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
		vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
		vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
		vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
		vertexBufferCreationDesc.m_isRawAccess = false;

		m_vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc, &triangleVertices);
	}
	void Renderer::RenderTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();
	}
}