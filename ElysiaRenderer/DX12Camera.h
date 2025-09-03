#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12Camera
	{
	public:
		DX12Camera() = default;
		DX12Camera(DX12Camera&& rhs) = default;

		float GetNearZ()
		{
			return m_nearZ;
		}
		float GetFarZ()
		{
			return m_farZ;
		}
		float GetFOV()
		{
			return m_FOV;
		}
		const XMVECTOR& GetCameraPos()
		{
			return m_cameraPos;
		}

		void SetCameraPos(XMVECTOR cameraPos)
		{
			m_cameraPos = cameraPos;
		}
		void SetCameraFOV(float FOV)
		{
			m_FOV = FOV;
		}
		void SetCameraNearZ(float nearZ)
		{
			m_nearZ = nearZ;
		}
		void SetCameraFarz(float farZ)
		{
			m_farZ = farZ;
		}

	private:
		float m_nearZ = 0.01f;
		float m_farZ = 100.f;
		float m_FOV = 0.8f;
		XMVECTOR m_cameraPos;
	};
}