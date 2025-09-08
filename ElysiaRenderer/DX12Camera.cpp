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
	void DX12Camera::LookAt(const FXMVECTOR& pos, const FXMVECTOR& up, const FXMVECTOR& target)
	{
		auto viewMat = XMMatrixLookAtLH(pos, target, up);
		XMStoreFloat4x4(&m_view, viewMat);

		auto lookVec = XMVector4Normalize(XMVectorSubtract(target, pos));
		auto rightVec = XMVector4Normalize(XMVector3Cross(lookVec, up));
		auto upVec = XMVector4Normalize(XMVector3Cross(lookVec, rightVec));

		XMStoreFloat4(&m_up, upVec);
		XMStoreFloat4(&m_right, rightVec);
		XMStoreFloat4(&m_look, lookVec);
		XMStoreFloat4(&m_cameraPos, pos);

		m_viewDirty = true;
	}
	void DX12Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& up, const XMFLOAT3& target)
	{
		auto temp = XMFLOAT4(pos.x, pos.y, pos.z, 1.f);
		auto posVec = XMLoadFloat4(&temp);

		temp = XMFLOAT4(up.x, up.y, up.z, 0.f);
		auto upVec = XMLoadFloat4(&temp);

		temp = XMFLOAT4(target.x, target.y, target.z, 1.f);
		auto targetVec = XMLoadFloat4(&temp);

		LookAt(posVec, upVec, targetVec);
	}

	void DX12Camera::MoveHorizon(float distance)
	{
		FXMVECTOR distanceVec = XMVectorReplicate(distance);
		FXMVECTOR rightVec = XMLoadFloat4(&m_right);
		FXMVECTOR posVec = XMLoadFloat4(&m_cameraPos);

		XMVECTOR movedPos = XMVectorMultiplyAdd(rightVec, distanceVec, posVec);
		XMStoreFloat4(&m_cameraPos, movedPos);

		m_viewDirty = true;
	}
	void DX12Camera::MoveVertical(float distance)
	{
		FXMVECTOR distanceVec = XMVectorReplicate(distance);
		FXMVECTOR lookVec = XMLoadFloat4(&m_look);
		FXMVECTOR posVec = XMLoadFloat4(&m_cameraPos);

		XMVECTOR movedPos = XMVectorMultiplyAdd(lookVec, distanceVec, posVec);
		XMStoreFloat4(&m_cameraPos, movedPos);

		m_viewDirty = true;
	}

	void DX12Camera::Pitch(float angle)
	{
		XMVECTOR rightVec = XMLoadFloat4(&m_right);
		XMVECTOR upVec = XMLoadFloat4(&m_up);
		XMVECTOR lookVec = XMLoadFloat4(&m_look);

		auto rotationMat = XMMatrixRotationAxis(rightVec, angle);

		upVec = XMVector4Transform(upVec, rotationMat);
		lookVec = XMVector4Transform(lookVec, rotationMat);

		XMStoreFloat4(&m_up, upVec);
		XMStoreFloat4(&m_look, lookVec);

		m_viewDirty = true;
	}
	void DX12Camera::Yaw(float angle)
	{
		XMVECTOR rightVec = XMLoadFloat4(&m_right);
		XMVECTOR lookVec = XMLoadFloat4(&m_look);

		auto rotationMat = XMMatrixRotationY(angle);

		rightVec = XMVector4Transform(rightVec, rotationMat);
		lookVec = XMVector4Transform(lookVec, rotationMat);

		XMStoreFloat4(&m_right, rightVec);
		XMStoreFloat4(&m_look, lookVec);

		m_viewDirty = true;
	}
	void DX12Camera::UpdateViewMatrix()
	{
		if (m_viewDirty)
		{
			XMVECTOR rightVec = XMVector4Normalize(XMLoadFloat4(&m_right));
			XMVECTOR upVec = XMVector4Normalize(XMLoadFloat4(&m_up));
			XMVECTOR lookVec = XMVector4Normalize(XMLoadFloat4(&m_look));
			XMVECTOR posVec = XMVector4Normalize(XMLoadFloat4(&m_cameraPos));

			upVec = XMVector4Normalize(XMVector3Cross(lookVec, rightVec));
			rightVec = XMVector4Normalize(XMVector3Cross(upVec, lookVec));

			float x = -XMVectorGetX(XMVector3Dot(posVec, rightVec));
			float y = -XMVectorGetX(XMVector3Dot(posVec, upVec));
			float z = -XMVectorGetX(XMVector3Dot(posVec, lookVec));

			XMStoreFloat4(&m_right, rightVec);
			XMStoreFloat4(&m_up, upVec);
			XMStoreFloat4(&m_look, lookVec);

			m_view(0, 0) = m_right.x;
			m_view(1, 0) = m_right.y;
			m_view(2, 0) = m_right.z;
			m_view(3, 0) = x;

			m_view(0, 1) = m_up.x;
			m_view(1, 1) = m_up.y;
			m_view(2, 1) = m_up.z;
			m_view(3, 1) = y;

			m_view(0, 2) = m_look.x;
			m_view(1, 2) = m_look.y;
			m_view(2, 2) = m_look.z;
			m_view(3, 2) = z;

			m_view(0, 3) = 0.0f;
			m_view(1, 3) = 0.0f;
			m_view(2, 3) = 0.0f;
			m_view(3, 3) = 1.0f;

			m_viewDirty = false;
		}
	}
}