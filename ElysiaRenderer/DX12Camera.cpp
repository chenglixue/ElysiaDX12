#include "stdafx.h"
#include "DX12Camera.h"

namespace ElysiaRenderer
{
	DX12Camera::DX12Camera(const Transform& transform, float aspectRatio, float fovy, float nearZ, float farZ) noexcept
		: m_transform(transform), 
		m_aspectRatio(aspectRatio), 
		m_fovy(fovy), 
		m_nearZ(nearZ), 
		m_farZ(farZ)
	{
		UpdateViewMatrix();
		UpdateProjMatrix();
	}
	DX12Camera::~DX12Camera()
	{

	}

	Transform	DX12Camera::GetTransform()		const noexcept
	{
		return m_transform;
	}

	float		DX12Camera::GetCameraSpeed()	const noexcept
	{
		return m_speed;
	}

	Vector3		DX12Camera::GetPosition()		const noexcept
	{
		return m_transform.m_position;
	}

	Vector4		DX12Camera::GetPosition4()		const noexcept
	{
		return Vector4(m_transform.m_position.x, m_transform.m_position.y, m_transform.m_position.z, 1);
	}

	Quaternion	DX12Camera::GetRotation()		const noexcept
	{
		return m_transform.m_rotation;
	}

	float		DX12Camera::GetNearZ()			const noexcept
	{
		return m_nearZ;
	}

	float		DX12Camera::GetFarZ()			const noexcept
	{
		return m_farZ;
	}

	float		DX12Camera::GetFOVY()			const noexcept
	{
		return m_fovy;
	}

	float		DX12Camera::GetAspect()			const noexcept
	{
		return m_aspectRatio;
	}

	Matrix		DX12Camera::GetViewMat()		const noexcept
	{
		return m_viewMatrix;
	}

	Matrix		DX12Camera::GetProj()			const noexcept
	{
		return m_projMatrix;
	}

	Vector3		DX12Camera::GetForwardDir()		const noexcept
	{
		return m_viewMatrix.Forward();
	}

	Vector3		DX12Camera::GetUpDir()			const noexcept
	{
		return m_viewMatrix.Up();
	}

	Vector3		DX12Camera::GetRightDir()		const noexcept
	{
		return m_viewMatrix.Right();
	}

	void		DX12Camera::SetCameraSpeed(float speed)				noexcept
	{
		m_speed = speed;
	}

	void		DX12Camera::SetPosition(const Vector3& cameraPos)	noexcept
	{
		m_transform.m_position = cameraPos;
		UpdateViewMatrix();
	}

	void		DX12Camera::SetRotation(const Quaternion& rotation) noexcept
	{
		m_transform.m_rotation = rotation;
		UpdateViewMatrix();
	}

	void		DX12Camera::SetAspectRatio(float aspectRatio)		noexcept
	{
		m_aspectRatio = aspectRatio;
		UpdateProjMatrix();
	}

	void		DX12Camera::SetNearZ(float nearZ)					noexcept
	{
		m_nearZ = nearZ;
		UpdateProjMatrix();
	}

	void		DX12Camera::SetFarz(float farZ)						noexcept
	{
		m_farZ = farZ;
		UpdateProjMatrix();
	}

	void		DX12Camera::Setfovy(float fovy)						noexcept
	{
		m_fovy = fovy;
		UpdateProjMatrix();
	}

	void		DX12Camera::LookAt(const Vector3& targetPos) noexcept
	{
		Vector3 up = Vector3::Up;
		
		m_viewMatrix = Matrix::CreateLookAt(m_transform.m_position, targetPos, up);
		auto inverseQuaternion = Quaternion::CreateFromRotationMatrix(m_viewMatrix.Invert());
		//m_transform.m_rotation.Inverse(inverseQuaternion);
		m_transform.m_rotation = inverseQuaternion;
	}

	void		DX12Camera::Rotate(const Vector3& pitchYawRollOffset) noexcept
	{
		Quaternion temp1 = Quaternion(-pitchYawRollOffset.x, 0.f, 0.f, 1.f);
		Quaternion temp2 = Quaternion(Quaternion::CreateFromAxisAngle(Vector3::Up, -pitchYawRollOffset.y));
		m_transform.m_rotation = temp2 * m_transform.m_rotation * temp1;

		m_transform.m_rotation.Normalize();
		UpdateViewMatrix();
	}

	void		DX12Camera::Translate(const Vector3& translateOffset) noexcept
	{
		Vector3 right = m_viewMatrix.Right();
		Vector3 up = m_viewMatrix.Up();
		Vector3 forward = m_viewMatrix.Forward();

		m_transform.m_position += translateOffset.z * forward + translateOffset.x * right + translateOffset.y * up;
		UpdateViewMatrix();
	}

	void		DX12Camera::UpdateViewMatrix() noexcept
	{
		//m_viewMatrix = Matrix::CreateTranslation(-m_transform.m_position) * Matrix::CreateFromQuaternion(m_transform.m_rotation);

		LookAt(m_transform.m_position + Vector3(300.f, 0.f, 0.f));

		//m_viewMatrix = Matrix::CreateLookAt(m_transform.m_position, Vector3::Zero, Vector3::Up);
	}

	void		DX12Camera::UpdateProjMatrix() noexcept
	{
		m_projMatrix = Matrix::CreatePerspectiveFieldOfView(m_fovy, m_aspectRatio, m_nearZ, m_farZ);
	}
}