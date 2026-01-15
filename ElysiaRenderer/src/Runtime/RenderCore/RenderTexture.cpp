#include "stdafx.h"
#include "RenderTexture.h"

#include "Runtime/Core/DX12TextureBuffer.h"

namespace ElysiaRenderer
{
    RenderTexture::~RenderTexture()
    {
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
        case TextureDimension::Tex2D:
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
        typeFlag = desc.IsDepth ? typeFlag | TexTypeFlags::DSV : typeFlag | TexTypeFlags::RTV;
        if (desc.EnableRandomWrite)
        {
            typeFlag = typeFlag | TexTypeFlags::UAV;
        }

        m_format = desc.Format;
        m_isDepth = desc.IsDepth;

        m_handle = std::move(
            TextureManager::GetInstance().CreateTexture(resouceDesc, typeFlag, desc.Name));
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
        return m_format;
    }
    ID3D12Resource* RenderTexture::GetResource() const
    {
        return GetTexture()->GetResource();
    }
    UINT64 RenderTexture::GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice, UINT64 planeSlice) const
    {
        auto desc = GetTexture()->GetResourceDesc();
        return mipmapLevel + arraySlice * desc.MipLevels + planeSlice * desc.MipLevels * desc.DepthOrArraySize;
    }
    UINT RenderTexture::GetResourceHeapIndex() const noexcept
    {
        return GetTexture()->GetResourceHeapIndex();
    }

    UINT RenderTexture::GetUAVResourceHeapIndex(UINT index) const
    {
        return GetTexture()->GetUAVResourceHeapIndex(index);
    }
    UINT RenderTexture::GetSRVResourceHeapIndex(UINT index) const
    {
        return GetTexture()->GetSRVResourceHeapIndex(index);
    }


    bool IsRenderTextureReady(const std::vector<ElysiaRenderer::RenderTexture*> texs)
    {
        bool isReady = true;

        for (const auto& tex : texs)
        {
            if (tex->GetTexture() == nullptr)
            {
                ThrowRuntimeError("null tex resource");;
            }
            isReady &= tex->GetTexture()->GetIsReady();
        }

        return isReady;
    }
}