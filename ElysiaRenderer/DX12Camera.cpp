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
		/*auto viewMat = XMMatrixLookAtLH(pos, target, up);
		XMStoreFloat4x4(&m_view, viewMat);*/

		auto lookUnNor = XMVectorSubtract(target, pos);
		auto lookVec = XMVector3Normalize(lookUnNor);
		auto rightVec = XMVector3Normalize(XMVector3Cross(up, lookVec));
		auto upVec = XMVector3Cross(lookVec, rightVec);

		XMStoreFloat3(&m_lookUnNor, lookUnNor);
		XMStoreFloat3(&m_up, upVec);
		XMStoreFloat3(&m_right, rightVec);
		XMStoreFloat3(&m_look, lookVec);
		XMStoreFloat3(&m_cameraPos, pos);

		m_viewDirty = true;
	}
	void DX12Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& up, const XMFLOAT3& target)
	{
		auto temp = XMFLOAT3(pos.x, pos.y, pos.z);
		auto posVec = XMLoadFloat3(&temp);

		temp = XMFLOAT3(up.x, up.y, up.z);
		auto upVec = XMLoadFloat3(&temp);

		temp = XMFLOAT3(target.x, target.y, target.z);
		auto targetVec = XMLoadFloat3(&temp);

		LookAt(posVec, upVec, targetVec);
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
			upVec = XMVector3Normalize(XMVector3Cross(rightVec, lookVec));
			rightVec = XMVector3Cross(upVec, lookVec);

			float x = -XMVectorGetX(XMVector3Dot(posVec, rightVec));
			float y = -XMVectorGetX(XMVector3Dot(posVec, upVec));
			float z = -XMVectorGetX(XMVector3Dot(posVec, lookVec));

			XMStoreFloat3(&m_right, rightVec);
			XMStoreFloat3(&m_up, upVec);
			XMStoreFloat3(&m_look, lookVec);

			auto viewMat = XMMatrixLookAtLH(posVec, XMVectorAdd(posVec, lookVec), upVec);
			XMStoreFloat4x4(&m_view, viewMat);

			/*m_view(0, 0) = m_right.x;
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
			m_view(3, 3) = 1.0f;*/
		}
	}
}