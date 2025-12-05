#include "stdafx.h"
#include "RenderTexture.h"
#include "../DX12/DX12Device.h"

namespace ElysiaRenderer
{
	RenderTexture::~RenderTexture()
	{
		ShutDowm();
	}

	void RenderTexture::Init(DX12Device* pDevice, RenderTextureDesc desc)
	{
		ShutDowm();

		assert(desc.Width > 0);
		assert(desc.Height > 0);
		assert(desc.Format != DXGI_FORMAT_UNKNOWN);
		assert(desc.MSAASamples > 0);

		TexCreateDesc textureCreateDesc{};
		textureCreateDesc.m_resouceDesc.Width = desc.Width;
		textureCreateDesc.m_resouceDesc.Height = static_cast<UINT>(desc.Height);
		textureCreateDesc.m_resouceDesc.MipLevels = desc.MipmapLevels;
		switch (desc.Dimension)
		{
			case TextureDimension::Tex2D :
			{
				textureCreateDesc.m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				break;
			}
			case TextureDimension::Tex2DArray:
			{
				textureCreateDesc.m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				textureCreateDesc.m_resouceDesc.DepthOrArraySize = static_cast<UINT16>(desc.ArraySize);
				break;
			}
			case TextureDimension::Tex3D:
			{
				textureCreateDesc.m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
				break;
			}
			case TextureDimension::Cube:
			{
				textureCreateDesc.m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				textureCreateDesc.m_resouceDesc.DepthOrArraySize = 6;
				break;
			}
			default:
			{
				ThrowRuntimeError("Invalid Render Texture Type!");
				break;
			}
		}
		textureCreateDesc.m_resouceDesc.Format = desc.Format;
		textureCreateDesc.m_resouceDesc.SampleDesc.Count = static_cast<UINT>(desc.MSAASamples);
		textureCreateDesc.m_resouceDesc.SampleDesc.Quality = desc.MSAASamples > 1 ? StandardMSAAPattern : 0;

		textureCreateDesc.m_name = desc.Name;
		textureCreateDesc.m_typeFlag = TexTypeFlags::SRV;
		textureCreateDesc.m_typeFlag = desc.IsDepth ? 
			textureCreateDesc.m_typeFlag | TexTypeFlags::DSV : textureCreateDesc.m_typeFlag | TexTypeFlags::RTV;
		if (desc.EnableRandomWrite)
		{
			textureCreateDesc.m_typeFlag = textureCreateDesc.m_typeFlag | TexTypeFlags::UAV;
		}

		if (desc.IsDepth)
		{
			m_depthFormat = desc.Format;
			m_isDepth = true;
		}

		m_pTexture = std::move(pDevice->CreateTexture(textureCreateDesc));
	}

	void RenderTexture::ShutDowm()
	{
		if (m_pTexture)
		{
			m_pTexture.reset();
		}
	}

	DX12TextureResource* RenderTexture::GetTexture() const
	{
		return m_pTexture.get();
	}
	UINT64 RenderTexture::GetSRVIndex() const
	{
		return m_pTexture->GetSRVDescriptor().GetHeapIndex();
	}
	UINT64 RenderTexture::GetWidth() const
	{
		return m_pTexture->GetResourceDesc().Width;
	}
	UINT64 RenderTexture::GetHeight() const
	{
		return m_pTexture->GetResourceDesc().Height;
	}
	DXGI_FORMAT RenderTexture::GetFormat() const
	{
		return m_isDepth ? m_depthFormat : m_pTexture->GetResourceDesc().Format;
	}
	ID3D12Resource* RenderTexture::GetResource() const
	{
		return m_pTexture->GetResource();
	}
	UINT64 RenderTexture::GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice) const
	{
		return arraySlice * m_pTexture->GetResourceDesc().MipLevels + mipmapLevel;
	}
}