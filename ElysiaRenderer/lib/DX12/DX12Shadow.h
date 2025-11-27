#pragma once
#include "../Utility/ShadowUtility.h"

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

		const UINT& GetWidth() const noexcept;
		const UINT& GetHeight() const noexcept;
		const DX12TextureResource* GetShadowRT() const noexcept;
		const D3D12_VIEWPORT& GetViewport() const noexcept;
		const D3D12_RECT& GetScissorRect() const noexcept;
		const float& GetNearZ() const noexcept;
		const float& GetFarZ() const noexcept;
		const Matrix& GetView() const noexcept;
		const Matrix& GetProj() const noexcept;
		const Matrix& GetShadowMat() const noexcept;

		void CreateViewport();
		void CreateScissorRect();

		void InitBoundSphere(float radius, Vector3 center = Vector3::Zero);
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