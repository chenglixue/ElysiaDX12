#pragma once
#include "ShadowUtility.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12Light;
	class DX12TextureResource;

	class DX12Shadow
	{
	public:
		DX12Shadow() = default;
		DX12Shadow(DX12TextureResource* buffer);
		DX12Shadow(const DX12Shadow& rhs) = delete;
		DX12Shadow& operator=(const DX12Shadow& rhs) = delete;
		DX12Shadow(DX12Shadow&& rhs) = default;
		~DX12Shadow();

		UINT GetWidth() const;
		UINT GetHeight() const;
		DX12TextureResource* GetShadowRT() const;
		D3D12_VIEWPORT& GetViewport();
		D3D12_RECT& GetScissorRect();
		float& GetNearZ();
		float& GetFarZ();
		Matrix& GetView();
		Matrix& GetProj();
		Matrix& GetShadowMat();

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

		DX12TextureResource* m_buffer = nullptr;
		BoundingSphere m_shadowBound;
		Vector3 m_lightPos;
		float m_nearZ;
		float m_farZ;
		Matrix m_shadowMatrix = Matrix::Identity;
		Matrix m_shadowViewMatrix = Matrix::Identity;
		Matrix m_shadowProjMatrix = Matrix::Identity;
	};
}