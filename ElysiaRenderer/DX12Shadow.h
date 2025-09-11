#pragma once
#include "stdafx.h"
#include "DX12TextureResource.h"
#include "DX12Light.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

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
		float& GetNearZ()
		{
			return m_nearZ;
		}
		float& GetFarZ()
		{
			return m_farZ;
		}
		const XMFLOAT4X4& GetView4X4() const
		{
			return m_shadowViewMatrix;
		}
		const XMFLOAT4X4& GetProj4X4() const
		{
			return m_shadowProjMatrix;
		}
		const XMFLOAT4X4& GetShadow4X4() const
		{
			return m_shadowMatrix;
		}

		void CreateViewport();
		void CreateScissorRect();

		void InitBoundSphere(float radius, XMFLOAT3 center = MathHelper::XMFLOAT3Zero());
		void UpdateShadowTransform(DX12Light* light);

	protected:

		UINT m_width;
		UINT m_height;
		DXGI_FORMAT m_format;

		D3D12_VIEWPORT m_viewPort{};
		D3D12_RECT m_scissorRect{};

		std::unique_ptr<DX12TextureResource> m_buffer;
		BoundingSphere m_shadowBound;
		XMFLOAT3 m_lightPos;
		float m_nearZ;
		float m_farZ;
		XMFLOAT4X4 m_shadowMatrix;
		XMFLOAT4X4 m_shadowViewMatrix;
		XMFLOAT4X4 m_shadowProjMatrix;
	};
}