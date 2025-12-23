#include "stdafx.h"
#include "RenderTexture.h"

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

		D3D12_RESOURCE_DESC resouceDesc{};
		resouceDesc.Width = desc.Width;
		resouceDesc.Height = static_cast<UINT>(desc.Height);
		resouceDesc.MipLevels = desc.MipmapLevels;
		switch (desc.Dimension)
		{
			case TextureDimension::Tex2D :
			{
				resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				break;
			}
			case TextureDimension::Tex2DArray:
			{
				resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resouceDesc.DepthOrArraySize = static_cast<UINT16>(desc.ArraySize);
				break;
			}
			case TextureDimension::Tex3D:
			{
				resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
				break;
			}
			case TextureDimension::Cube:
			{
				resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resouceDesc.DepthOrArraySize = 6;
				break;
			}
			default:
			{
				ThrowRuntimeError("Invalid Render Texture Type!");
				break;
			}
		}
		resouceDesc.Format = desc.Format;
		resouceDesc.SampleDesc.Count = static_cast<UINT>(desc.MSAASamples);
		resouceDesc.SampleDesc.Quality = desc.MSAASamples > 1 ? StandardMSAAPattern : 0;
		resouceDesc.Alignment = 0;
		resouceDesc.DepthOrArraySize = desc.ArraySize;
		resouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resouceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		TexTypeFlags typeFlag = TexTypeFlags::SRV;
		typeFlag = desc.IsDepth ? 
			typeFlag | TexTypeFlags::DSV : typeFlag | TexTypeFlags::RTV;
		if (desc.EnableRandomWrite)
		{
			typeFlag = typeFlag | TexTypeFlags::UAV;
		}

		if (desc.IsDepth)
		{
			m_depthFormat = desc.Format;
			m_isDepth = true;
		}

		m_handle = std::move(TextureManager::GetInstance().CreateTexture(resouceDesc, typeFlag, desc.Name));
	}

	void RenderTexture::ShutDowm()
	{
		if (m_handle.IsValid())
		{
			TextureManager::GetInstance().Release(m_handle);
		}
	}

	DX12TextureResource* RenderTexture::GetTexture() const
	{
		return TextureManager::GetInstance().GetTexture(m_handle);
	}
	UINT64 RenderTexture::GetSRVIndex() const
	{
		return GetTexture()->GetSRVDescriptor().GetHeapIndex();
	}
	UINT64 RenderTexture::GetWidth() const
	{
		return GetTexture()->GetResourceDesc().Width;
	}
	UINT64 RenderTexture::GetHeight() const
	{
		return GetTexture()->GetResourceDesc().Height;
	}
	DXGI_FORMAT RenderTexture::GetFormat() const
	{
		return m_isDepth ? m_depthFormat : GetTexture()->GetResourceDesc().Format;
	}
	ID3D12Resource* RenderTexture::GetResource() const
	{
		return GetTexture()->GetResource();
	}
	UINT64 RenderTexture::GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice) const
	{
		return arraySlice * GetTexture()->GetResourceDesc().MipLevels + mipmapLevel;
	}
	UINT RenderTexture::GetResourceHeapIndex() const noexcept
	{
		return GetTexture()->GetResourceHeapIndex();
	}

}