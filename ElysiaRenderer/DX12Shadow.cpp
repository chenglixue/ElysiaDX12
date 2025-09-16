#include "DX12Shadow.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	DX12Shadow::DX12Shadow(std::shared_ptr<DX12TextureResource> buffer)
		: m_buffer(buffer)
	{
		m_width = static_cast<UINT>(m_buffer->GetResourceDesc().Width);
		m_height = m_buffer->GetResourceDesc().Height;
		m_format = m_buffer->GetResourceDesc().Format;

		ZeroMemory(&m_viewPort, sizeof(D3D12_VIEWPORT));
		ZeroMemory(&m_scissorRect, sizeof(D3D12_RECT));

		CreateViewport();
		CreateScissorRect();
	}

	DX12Shadow::~DX12Shadow()
	{

	}

	void DX12Shadow::CreateViewport()
	{
		m_viewPort.Width = static_cast<float>(m_width);
		m_viewPort.Height = static_cast<float>(m_height);
		m_viewPort.TopLeftX = 0;
		m_viewPort.TopLeftY = 0;
		m_viewPort.MinDepth = 0;
		m_viewPort.MaxDepth = 1;
	}
	void DX12Shadow::CreateScissorRect()
	{
		m_scissorRect.left = 0;
		m_scissorRect.right = m_width;
		m_scissorRect.bottom = m_height;
		m_scissorRect.top = 0;
	}

	void DX12Shadow::InitBoundSphere(float radius, XMFLOAT3 center)
	{
		m_shadowBound.Center = center;
		m_shadowBound.Radius = radius;
	}

	void DX12Shadow::UpdateShadowTransform(DX12Light* light)
	{
		XMMATRIX o = XMMatrixIdentity();

		auto lightDir = XMLoadFloat3(&light->GetLightDir());
		auto lightPos = 2.f * m_shadowBound.Radius * (-lightDir);
		auto boundPosWS = XMLoadFloat3(&m_shadowBound.Center);
		auto lightUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
		auto lightViewMat = XMMatrixLookAtLH(lightPos, boundPosWS, lightUp);

		XMStoreFloat3(&m_lightPos, lightPos);

		XMFLOAT3 boundPosLS = MathHelper::XMFLOAT3Zero();
		XMStoreFloat3(&boundPosLS, XMVector3TransformCoord(boundPosWS, lightViewMat));

		float l = boundPosLS.x - m_shadowBound.Radius;
		float r = boundPosLS.x + m_shadowBound.Radius;
		float t = boundPosLS.y + m_shadowBound.Radius;
		float b = boundPosLS.y - m_shadowBound.Radius;
		float n = boundPosLS.z - m_shadowBound.Radius;
		float f = boundPosLS.z + m_shadowBound.Radius;
		m_nearZ = n;
		m_farZ = f;
		auto LS2ProjMat = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		XMStoreFloat4x4(&m_shadowMatrix, lightViewMat * LS2ProjMat);
		XMStoreFloat4x4(&m_shadowViewMatrix, lightViewMat);
		XMStoreFloat4x4(&m_shadowProjMatrix, LS2ProjMat);
	}
}