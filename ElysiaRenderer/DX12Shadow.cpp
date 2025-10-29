#include "stdafx.h"
#include "Helper.h"
#include "DX12Shadow.h"
#include "DX12Light.h"
#include "DX12TextureBuffer.h"


namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	DX12Shadow::DX12Shadow(DX12TextureResource* buffer)
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

	UINT DX12Shadow::GetWidth() const
	{
		return m_width;
	}
	UINT DX12Shadow::GetHeight() const
	{
		return m_height;
	}
	DX12TextureResource* DX12Shadow::GetShadowRT() const
	{
		return m_buffer;
	}
	D3D12_VIEWPORT& DX12Shadow::GetViewport()
	{
		return m_viewPort;
	}
	D3D12_RECT& DX12Shadow::GetScissorRect()
	{
		return m_scissorRect;
	}
	float& DX12Shadow::GetNearZ()
	{
		return m_nearZ;
	}
	float& DX12Shadow::GetFarZ()
	{
		return m_farZ;
	}
	Matrix& DX12Shadow::GetView()
	{
		return m_shadowViewMatrix;
	}
	Matrix& DX12Shadow::GetProj()
	{
		return m_shadowProjMatrix;
	}
	Matrix& DX12Shadow::GetShadowMat()
	{
		return m_shadowMatrix;
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

	void DX12Shadow::InitBoundSphere(float radius, Vector3 center)
	{
		m_shadowBound.Center = center;
		m_shadowBound.Radius = radius;
	}

	void DX12Shadow::UpdateShadowTransform(DX12Light* light)
	{
		Vector3 lightDir = light->GetLightDir();
		m_lightPos = -lightDir * 2.f * m_shadowBound.Radius;
		Vector3 boundPosWS = m_shadowBound.Center;
		Vector3 lightUp = Vector3::Up;
		Matrix lightViewMat = Matrix::CreateLookAt(m_lightPos, boundPosWS, lightUp); //XMMatrixLookAtLH(m_lightPos, boundPosWS, lightUp);

		Vector3 boundPosLS = Vector3::Zero;
		Vector3::Transform(boundPosWS, lightViewMat, boundPosLS);

		float l = boundPosLS.x - m_shadowBound.Radius;
		float r = boundPosLS.x + m_shadowBound.Radius;
		float t = boundPosLS.y + m_shadowBound.Radius;
		float b = boundPosLS.y - m_shadowBound.Radius;
		float n = boundPosLS.z - m_shadowBound.Radius;
		float f = boundPosLS.z + m_shadowBound.Radius;
		m_nearZ = n;
		m_farZ = f;
		auto LS2ProjMat = Matrix::CreateOrthographicOffCenter(l, r, b, t, n, f);

		m_shadowMatrix = lightViewMat * LS2ProjMat;
		m_shadowViewMatrix = lightViewMat;
		m_shadowProjMatrix = LS2ProjMat;
	}
}