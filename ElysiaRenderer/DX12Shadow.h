#pragma once
#include "stdafx.h"
#include "DX12TextureResource.h"
#include "DX12Light.h"

namespace ElysiaRenderer
{
	class DX12Shadow
	{
	public:
		DX12Shadow() = default;
		DX12Shadow(std::unique_ptr<DX12TextureResource> buffer);
		DX12Shadow(const DX12Shadow& rhs) = delete;
		DX12Shadow& operator=(const DX12Shadow& rhs) = delete;
		DX12Shadow(DX12Shadow&& rhs) = default;
		~DX12Shadow();

		UINT GetWidth() const
		{
			return m_width;
		}
		UINT GetHeight() const
		{
			return m_height;
		}
		DX12TextureResource* GetShadowRT() const
		{
			return m_buffer.get();
		}
		D3D12_VIEWPORT& GetViewport()
		{
			return m_viewPort;
		}
		D3D12_RECT& GetScissorRect()
		{
			return m_scissorRect;
		}

		void CreateViewport();
		void CreateScissorRect();

		XMMATRIX UpdateShadowTransform(DX12Light* light);

	protected:
		

		UINT m_width;
		UINT m_height;
		DXGI_FORMAT m_format;

		D3D12_VIEWPORT m_viewPort{};
		D3D12_RECT m_scissorRect{};

		std::unique_ptr<DX12TextureResource> m_buffer;
		BoundingSphere m_shadowBound;
		XMFLOAT3 m_lightPos;
	};
}