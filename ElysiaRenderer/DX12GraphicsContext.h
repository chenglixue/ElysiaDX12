#pragma once
#include "DX12Context.h"
#include "stdafx.h"
#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
	extern class DX12Device;
	using namespace DirectX::SimpleMath;

	class DX12GraphicsContext : public DX12Context
	{
	public:
		DX12GraphicsContext(DX12Device* device);
		~DX12GraphicsContext() override;

		void ClearRenderTarget(const DX12TextureResource& renderTarget, Color color);
		void ClearDepthStencilTarget(const DX12TextureResource& renderTarget, float depth, uint8_t stencil);

		void SetDefaultViewportAndScissor(ElysiaHelper::UINT2 screenSize);
		void SetViewport(D3D12_VIEWPORT& viewPort);
		void SetScissorRect(D3D12_RECT& rect);

	private:
	};
}