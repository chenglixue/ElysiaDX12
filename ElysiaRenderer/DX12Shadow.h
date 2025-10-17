#pragma once
#include "stdafx.h"
#include "DX12TextureBuffer.h"
#include "DX12Light.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	enum class ShadowQuality : uint8_t
	{
		Low = 0,
		Middle = 1,
		High = 2,
		VeryHigh = 3
	};

	enum class ShadowType : uint8_t
	{
		Hard = 0,
		Soft = 1
	};

	class DX12Shadow
	{
	public:
		DX12Shadow() = default;
		DX12Shadow(std::shared_ptr<DX12TextureResource> buffer);
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
		Matrix& GetView()
		{
			return m_shadowViewMatrix;
		}
		Matrix& GetProj()
		{
			return m_shadowProjMatrix;
		}
		Matrix& GetShadowMat()
		{
			return m_shadowMatrix;
		}

		void CreateViewport();
		void CreateScissorRect();

		void InitBoundSphere(float radius, Vector3 center = MathHelper::XMFLOAT3Zero());
		void UpdateShadowTransform(DX12Light* light);

	protected:

		UINT m_width;
		UINT m_height;
		DXGI_FORMAT m_format;

		D3D12_VIEWPORT m_viewPort{};
		D3D12_RECT m_scissorRect{};

		std::shared_ptr<DX12TextureResource> m_buffer;
		BoundingSphere m_shadowBound;
		Vector3 m_lightPos;
		float m_nearZ;
		float m_farZ;
		Matrix m_shadowMatrix = Matrix::Identity;
		Matrix m_shadowViewMatrix = Matrix::Identity;
		Matrix m_shadowProjMatrix = Matrix::Identity;
	};
}