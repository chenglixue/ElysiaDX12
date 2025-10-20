#include "RenderTexture.h"

namespace ElysiaHelper
{
	RenderTexture::~RenderTexture()
	{
		ShutDowm();
	}

	void RenderTexture::Init(RenderTextureDesc desc)
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
		switch (textureCreateDesc.m_resouceDesc.Dimension)
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
		textureCreateDesc.m_resouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureCreateDesc.m_resouceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureCreateDesc.m_resouceDesc.Alignment = 0;
		textureCreateDesc.m_resouceDesc.SampleDesc.Count = static_cast<UINT>(desc.MSAASamples);
		textureCreateDesc.m_resouceDesc.SampleDesc.Quality = desc.MSAASamples > 1 ? StandardMSAAPattern : 0;

		textureCreateDesc.m_name = desc.Name;
		textureCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::RTV;
		if (desc.EnableRandomWrite)
		{
			textureCreateDesc.m_typeFlag = textureCreateDesc.m_typeFlag | TexTypeFlags::UAV;
		}

		m_pTexture = std::move(Renderer::GetDevice()->CreateTexture(textureCreateDesc));
	}

	void RenderTexture::ShutDowm()
	{
		if (m_pTexture)
		{
			m_pTexture.reset();
		}
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
		return m_pTexture->GetResourceDesc().Format;
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