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
		 void RenderClearColor();
		 void RenderTriangle();

		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
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

	void Renderer::RenderTriangle()
	{

	}
}