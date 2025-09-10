#include "DX12Shadow.h"

namespace ElysiaRenderer
{
	DX12Shadow::DX12Shadow(std::unique_ptr<DX12TextureResource> buffer)
		: m_buffer(std::move(buffer))
	{
		m_width = m_buffer->GetResourceDesc().Width;
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

	XMMATRIX DX12Shadow::UpdateShadowTransform(DX12Light* light)
	{
		XMMATRIX o = XMMatrixIdentity();

		auto lightDir = XMLoadFloat3(&light->GetLightDir());
		auto lightPos = 2.f * m_shadowBound.Radius * (-lightDir);
		auto targetPos = XMLoadFloat3(&m_shadowBound.Center);
		auto lightUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
		auto lightViewMat = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

		XMStoreFloat3(&m_lightPos, lightPos);

		return o;
	}
}