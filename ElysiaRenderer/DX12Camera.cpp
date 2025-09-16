#include "DX12Camera.h"

namespace ElysiaRenderer
{
	DX12Camera::~DX12Camera()
	{

	}

	void DX12Camera::SetLens(float FOVY, float aspect, float nearZ, float farZ)
	{
		m_FOVY = FOVY;
		m_aspect = aspect;
		m_nearZ = nearZ;
		m_farZ = farZ;

		m_nearHeight = 2.f * m_nearZ * tanf(0.5f * m_FOVY);
		m_farHeight = 2.f * m_farZ * tanf(0.5f * m_FOVY);

		auto projMat = XMMatrixPerspectiveFovLH(m_FOVY, m_aspect, m_nearZ, m_farZ);
		XMStoreFloat4x4(&m_proj, projMat);
	}
	void DX12Camera::LookAt(const Vector3& pos, const Vector3& up, const Vector3& target)
	{
		/*auto viewMat = XMMatrixLookAtLH(pos, target, up);
		XMStoreFloat4x4(&m_view, viewMat);*/

		auto lookUnNor = target - pos;
		auto lookNor = lookUnNor;
		lookNor.Normalize();
		auto rightVec = up.Cross(lookNor);
		auto upVec = XMVector3Cross(lookNor, rightVec);

		m_lookUnNor = lookUnNor;
		m_up = upVec;
		m_right = rightVec;
		m_look = lookNor;
		m_cameraPos = pos;

		m_viewDirty = true;
	}

	void DX12Camera::MoveHorizon(float distance)
	{
		FXMVECTOR distanceVec = XMVectorReplicate(distance);
		FXMVECTOR rightVec = XMLoadFloat3(&m_right);
		FXMVECTOR posVec = XMLoadFloat3(&m_cameraPos);

		XMVECTOR movedPos = XMVectorMultiplyAdd(rightVec, distanceVec, posVec);
		XMStoreFloat3(&m_cameraPos, movedPos);

		m_viewDirty = true;
	}
	void DX12Camera::MoveVertical(float distance)
	{
		FXMVECTOR distanceVec = XMVectorReplicate(distance);
		FXMVECTOR lookVec = XMLoadFloat3(&m_look);
		FXMVECTOR posVec = XMLoadFloat3(&m_cameraPos);

		XMVECTOR movedPos = XMVectorMultiplyAdd(lookVec, distanceVec, posVec);
		XMStoreFloat3(&m_cameraPos, movedPos);

		m_viewDirty = true;
	}

	void DX12Camera::Pitch(float angle)
	{
		XMVECTOR rightVec = XMLoadFloat3(&m_right);
		XMVECTOR upVec = XMLoadFloat3(&m_up);
		XMVECTOR lookVec = XMLoadFloat3(&m_look);

		auto rotationMat = XMMatrixRotationAxis(rightVec, angle);

		upVec = XMVector3TransformNormal(upVec, rotationMat);
		lookVec = XMVector3TransformNormal(lookVec, rotationMat);

		XMStoreFloat3(&m_up, upVec);
		XMStoreFloat3(&m_look, lookVec);

		m_viewDirty = true;
	}
	void DX12Camera::Yaw(float angle)
	{
		XMVECTOR rightVec = XMLoadFloat3(&m_right);
		XMVECTOR lookVec = XMLoadFloat3(&m_look);
		XMVECTOR upVec = XMLoadFloat3(&m_up);

		auto rotationMat = XMMatrixRotationY(angle);

		rightVec = XMVector3TransformNormal(rightVec, rotationMat);
		lookVec = XMVector3TransformNormal(lookVec, rotationMat);
		upVec = XMVector3TransformNormal(upVec, rotationMat);

		XMStoreFloat3(&m_right, rightVec);
		XMStoreFloat3(&m_look, lookVec);
		XMStoreFloat3(&m_up, upVec);

		m_viewDirty = true;
	}
	void DX12Camera::UpdateViewMatrix()
	{
		if (m_viewDirty)
		{
			XMVECTOR rightVec = XMLoadFloat3(&m_right);
			XMVECTOR upVec = XMLoadFloat3(&m_up);
			XMVECTOR lookVec = XMLoadFloat3(&m_look);
			XMVECTOR posVec = XMLoadFloat3(&m_cameraPos);

			lookVec = XMVector3Normalize(lookVec);
			upVec = XMVector3Normalize(XMVector3Cross(lookVec, rightVec));
			rightVec = XMVector3Cross(upVec, lookVec);

			float x = -XMVectorGetX(XMVector3Dot(posVec, rightVec));
			float y = -XMVectorGetX(XMVector3Dot(posVec, upVec));
			float z = -XMVectorGetX(XMVector3Dot(posVec, lookVec));

			XMStoreFloat3(&m_right, rightVec);
			XMStoreFloat3(&m_up, upVec);
			XMStoreFloat3(&m_look, lookVec);

			auto viewMat = XMMatrixLookAtLH(posVec, XMVectorAdd(posVec, lookVec), upVec);
			XMStoreFloat4x4(&m_view, viewMat);
		}
	}
}