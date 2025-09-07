#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12Camera
	{
	public:
		DX12Camera() = default;
		DX12Camera(DX12Camera&& rhs) = default;
		~DX12Camera();

		XMVECTOR GetCameraPos() const
		{
			return m_cameraPos;
		}
		XMFLOAT3 GetCameraPosF() const
		{
			XMFLOAT3 cameraPosF;
			XMStoreFloat3(&cameraPosF, m_cameraPos);

			return cameraPosF;
		}
		void SetCameraPos(XMFLOAT3 cameraPos) const
		{
			m_cameraPos = XMVectorSet(cameraPos.x, cameraPos.y, cameraPos.z, 1.f);
		}
		void SetCameraPos(float x, float y, float z) const
		{
			m_cameraPos = XMVectorSet(x, y, z, 1.f);
		}

		float GetNearZ() const
		{
			return m_nearZ;
		}
		float GetFarZ() const
		{
			return m_farZ;
		}
		void SetCameraNearZ(float nearZ) const
		{
			m_nearZ = nearZ;
		}
		void SetCameraFarz(float farZ) const
		{
			m_farZ = farZ;
		}

		float GetFOV() const
		{
			return m_FOV;
		}
		void SetCameraFOV(float FOV) const
		{
			m_FOV = FOV;
		}

		XMVECTOR GetRight() const
		{
			return m_right;
		}
		XMFLOAT4 GetRightF() const
		{
			XMFLOAT4 o;
			XMStoreFloat4(&o, m_right);

			return o;
		}
		XMVECTOR SetRight(XMFLOAT3 right) const
		{
			XMVectorSet(&m_right, )
			m_right = right;
		}

	private:
		float m_nearZ = 0.01f;
		float m_farZ = 100.f;
		float m_FOV = 0.8f;

		XMFLOAT4 m_cameraPos;
		XMFLOAT4 m_right;
		XMFLOAT4 m_up;
		XMFLOAT4 m_look;
	};
}