#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;

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

		Vector3 GetCameraPos() const
		{
			return m_cameraPos;
		}
		void SetCameraPos(const Vector3& cameraPos)
		{
			m_cameraPos = cameraPos;
			m_viewDirty = true;
		}

		float& GetNearZ() 
		{
			return m_nearZ;
		}
		float& GetFarZ() 
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

		Vector3 GetRight() const
		{
			return m_right;
		}
		void SetRight(const Vector3& right)
		{
			m_right = right;
			m_viewDirty = true;
		}

		Vector3 GetUP() const
		{
			return m_up;
		}
		void SetUP(const Vector3& up)
		{
			m_up = up;
			m_viewDirty = true;
		}
		void SetUP(float x, float y, float z)
		{
			m_up = XMFLOAT3(x, y, z);
			m_viewDirty = true;
		}

		Vector3 GetLook() const
		{
			return m_look;
		}
		void SetLook(const Vector3& look)
		{
			m_look = look;
			m_viewDirty = true;
		}

		Matrix GetViewMat() const
		{
			return m_view;
		}
		Matrix GetProj() const
		{
			return m_proj;
		}

		// set proj & view matrix
		void SetLens(float FOVY, float aspect, float nearZ, float farZ);
		void LookAt(const Vector3& pos, const Vector3& up, const Vector3& target);

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

		Vector3 m_cameraPos = Vector3::Zero;
		Vector3 m_right = {1.f, 0.f, 0.f};
		Vector3 m_up = {0.f , 1.f, 0.f};
		Vector3 m_look = {0.f, 0.f, 1.f};
		Vector3 m_lookUnNor = { 0.f, 0.f, 1.f };

		Matrix  m_view = Matrix::Identity;
		Matrix  m_proj = Matrix::Identity;
	};
}