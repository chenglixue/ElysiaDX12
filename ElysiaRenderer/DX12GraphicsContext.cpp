#include "DX12GraphicsContext.h"


namespace ElysiaRenderer
{
	extern class DX12Device;

	DX12GraphicsContext::DX12GraphicsContext(DX12Device* device) : 
		DX12Context(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		 
	}

	DX12GraphicsContext::~DX12GraphicsContext()
	{

	}

	void DX12GraphicsContext::ClearRenderTarget(const DX12TextureResource& renderTarget, Color color)
	{
		m_commandList->ClearRenderTargetView(renderTarget.GetRTVDescriptor().GetCPUHandle(),
			color, 0, nullptr);
	}
	void DX12GraphicsContext::ClearDepthStencilTarget(const DX12TextureResource& renderTarget, float depth, uint8_t stencil)
	{
		m_commandList->ClearDepthStencilView(renderTarget.GetDSVDescriptor().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			depth, stencil, 0, nullptr);
	}

	void DX12GraphicsContext::SetDefaultViewportAndScissor(ElysiaHelper::UINT2 screenSize)
	{
		D3D12_VIEWPORT viewport = {};
		viewport.Width			= static_cast<float>(screenSize.x);
		viewport.Height			= static_cast<float>(screenSize.y);
		viewport.TopLeftX		= 0;
		viewport.TopLeftY		= 0;
		viewport.MinDepth		= 0;
		viewport.MaxDepth		= 1;

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.right = screenSize.x;
		scissorRect.bottom = 0;
		scissorRect.top = screenSize.y;

		SetViewport(viewport);
		SetScissorRect(scissorRect);
	}
	void DX12GraphicsContext::SetViewport(D3D12_VIEWPORT& viewPort)
	{
		m_commandList->RSSetViewports(1, &viewPort);
	}
	void DX12GraphicsContext::SetScissorRect(D3D12_RECT& rect)
	{
		m_commandList->RSSetScissorRects(1, &rect);
	}
}