#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12Camera
	{
	public:
		DX12Camera() = default;
		DX12Camera(const DX12Camera& rhs) = default;
		DX12Camera& operator=(const DX12Camera& rhs) = default;
		DX12Camera(DX12Camera&& rhs) = default;
		~DX12Camera();

		XMVECTOR GetCameraPos() const
		{
			return XMLoadFloat4(&m_cameraPos);
		}
		XMFLOAT4 GetCameraPosF() const
		{
			return m_cameraPos;
		}
		void SetCameraPos(const XMFLOAT4& cameraPos)
		{
			m_cameraPos = cameraPos;
		}
		void SetCameraPos(float x, float y, float z)
		{
			m_cameraPos = XMFLOAT4(x, y, z, 1.f);
		}
		void SetCameraPos(const XMVECTOR cameraPos)
		{
			XMStoreFloat4(&m_cameraPos, cameraPos);
		}

		float GetNearZ() const
		{
			return m_nearZ;
		}
		float GetFarZ() const
		{
			return m_farZ;
		}
		void SetCameraNearZ(float nearZ)
		{
			m_nearZ = nearZ;
		}
		void SetCameraFarz(float farZ)
		{
			m_farZ = farZ;
		}

		float GetFOVY() const
		{
			return m_FOVY;
		}
		float GetFOVX()
		{
			float halfNearWidth = 0.5f * GetNearWidth();
			return 2.f * atan(halfNearWidth / m_nearZ);
		}
		void SetCameraFOVY(float FOV)
		{
			m_FOVY = FOV;
		}

		float GetAspect() const
		{
			return m_aspect;
		}
		float GetNearWidth() const
		{
			return m_aspect * m_nearHeight;
		}
		float GetNearHeight() const
		{
			return m_nearHeight;
		}
		float GetFarWidth() const
		{
			return m_aspect * m_farHeight;
		}
		float GetFarHeight() const
		{
			return m_farHeight;
		}

		XMVECTOR GetRight() const
		{
			return XMLoadFloat4(&m_right);
		}
		XMFLOAT4 GetRightF() const
		{
			return m_right;
		}
		void SetRight(const XMFLOAT4& right)
		{
			m_right = right;
		}
		void SetRight(const XMVECTOR& right)
		{
			XMStoreFloat4(&m_right, right);
		}
		void SetRight(float x, float y, float z)
		{
			m_right = XMFLOAT4(x, y, z, 0.f);
		}

		XMVECTOR GetUP() const
		{
			return XMLoadFloat4(&m_up);
		}
		XMFLOAT4 GetUPF() const
		{
			return m_up;
		}
		void SetUP(const XMFLOAT4& up)
		{
			m_up = up;
		}
		void SetUP(const XMVECTOR& up)
		{
			XMStoreFloat4(&m_up, up);
		}
		void SetUP(float x, float y, float z)
		{
			m_up = XMFLOAT4(x, y, z, 0.f);
		}

		XMVECTOR GetLook() const
		{
			return XMLoadFloat4(&m_look);
		}
		XMFLOAT4 GetLookF() const
		{
			return m_look;
		}
		void SetLook(const XMFLOAT4& look)
		{
			m_look = look;
		}
		void SetLook(const XMVECTOR& look)
		{
			XMStoreFloat4(&m_look, look);
		}
		void SetLook(float x, float y, float z)
		{
			m_look = XMFLOAT4(x, y, z, 0.f);
		}

		XMMATRIX GetViewMat() const
		{
			return XMLoadFloat4x4(&m_view);
		}
		XMFLOAT4X4 GetView4X4F() const
		{
			return m_view;
		}
		XMMATRIX GetProjMat() const
		{
			return XMLoadFloat4x4(&m_proj);
		}
		XMFLOAT4X4 GetProj4X4F() const
		{
			return m_proj;
		}

		// set proj & view matrix
		void SetLens(float FOVY, float aspect, float nearZ, float farZ);
		void LookAt(const FXMVECTOR& pos, const FXMVECTOR& up, const FXMVECTOR& target);
		void LookAt(const XMFLOAT3& pos, const XMFLOAT3& up, const XMFLOAT3& target);

		// Move Camera
		void MoveHorizon(float distance);	// move left right
		void MoveVertical(float distance);	// move forward back

		// Rotate Camera
		void Pitch(float angle);
		void Yaw(float angle);
		void UpdateViewMatrix();	// after roate, three vec in camera would not Non-orthogonal normalized vector 

	private:
		bool m_viewDirty = false;	// view data whether change

		float m_aspect;
		float m_nearZ = 0.01f;
		float m_farZ = 100.f;
		float m_FOVY = 0.8f;
		float m_nearHeight = 0.f;
		float m_farHeight = 0.f;

		XMFLOAT4 m_cameraPos = {0.f, 0.f, 0.f, 1.f};
		XMFLOAT4 m_right = {1.f , 0.f, 0.f, 0.f};
		XMFLOAT4 m_up = {0.f , 1.f, 0.f, 0.f};
		XMFLOAT4 m_look = {0.f, 0.f, 1.f, 0.f};

		XMFLOAT4X4 m_view = MathHelper::Identity4x4();
		XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	};
}