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

		float& GetCameraSpeed()
		{
			return m_speed;
		}
		void SetCameraSpeed(float speed)
		{
			m_speed = speed;
		}

		XMVECTOR GetCameraPos() const
		{
			return XMLoadFloat3(&m_cameraPos);
		}
		XMFLOAT3 GetCameraPosF() const
		{
			return m_cameraPos;
		}
		void SetCameraPos(const XMFLOAT3& cameraPos)
		{
			m_cameraPos = cameraPos;
			m_viewDirty = true;
		}
		void SetCameraPos(float x, float y, float z)
		{
			m_cameraPos = XMFLOAT3(x, y, z);
			m_viewDirty = true;
		}
		void SetCameraPos(const XMVECTOR cameraPos)
		{
			XMStoreFloat3(&m_cameraPos, cameraPos);
			m_viewDirty = true;
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
			m_viewDirty = true;
		}
		void SetCameraFarz(float farZ)
		{
			m_farZ = farZ;
			m_viewDirty = true;
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
			m_viewDirty = true;
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
			return XMLoadFloat3(&m_right);
		}
		XMFLOAT3 GetRightF() const
		{
			return m_right;
		}
		void SetRight(const XMFLOAT3& right)
		{
			m_right = right;
			m_viewDirty = true;
		}
		void SetRight(const XMVECTOR& right)
		{
			XMStoreFloat3(&m_right, right);
			m_viewDirty = true;
		}
		void SetRight(float x, float y, float z)
		{
			m_right = XMFLOAT3(x, y, z);
			m_viewDirty = true;
		}

		XMVECTOR GetUP() const
		{
			return XMLoadFloat3(&m_up);
		}
		XMFLOAT3 GetUPF() const
		{
			return m_up;
		}
		void SetUP(const XMFLOAT3& up)
		{
			m_up = up;
			m_viewDirty = true;
		}
		void SetUP(const XMVECTOR& up)
		{
			XMStoreFloat3(&m_up, up);
			m_viewDirty = true;
		}
		void SetUP(float x, float y, float z)
		{
			m_up = XMFLOAT3(x, y, z);
			m_viewDirty = true;
		}

		XMVECTOR GetLook() const
		{
			return XMLoadFloat3(&m_look);
		}
		XMFLOAT3 GetLookF() const
		{
			return m_look;
		}
		void SetLook(const XMFLOAT3& look)
		{
			m_look = look;
			m_viewDirty = true;
		}
		void SetLook(const XMVECTOR& look)
		{
			XMStoreFloat3(&m_look, look);
			m_viewDirty = true;
		}
		void SetLook(float x, float y, float z)
		{
			m_look = XMFLOAT3(x, y, z);
			m_viewDirty = true;
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

		bool IsViewDirty() const
		{
			return m_viewDirty;
		}
		void DisableViewDirty()
		{
			m_viewDirty = false;
		}

	protected:
		bool m_viewDirty = false;	// view data whether change

		float m_speed = 1.f;

		float m_aspect;
		float m_nearZ = 0.01f;
		float m_farZ = 100.f;
		float m_FOVY = 0.8f;
		float m_nearHeight = 0.f;
		float m_farHeight = 0.f;

		XMFLOAT3 m_cameraPos = {0.f, 0.f, 0.f};
		XMFLOAT3 m_right = {1.f , 0.f, 0.f};
		XMFLOAT3 m_up = {0.f , 1.f, 0.f};
		XMFLOAT3 m_look = {0.f, 0.f, 1.f};
		XMFLOAT3 m_lookUnNor = { 0.f, 0.f, 1.f };

		XMFLOAT4X4 m_view = MathHelper::Identity4x4();
		XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	};
}