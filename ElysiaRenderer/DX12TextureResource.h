#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	enum class TexTypeFlags : uint8_t
	{
		None = 0,
		RTV = 1 << 0,
		DSV = 1 << 1,
		SRV = 1 << 2,
		UAV = 1 << 3
	};
	inline TexTypeFlags operator|(TexTypeFlags l, TexTypeFlags r)
	{
		return static_cast<TexTypeFlags>(static_cast<uint8_t>(l) | static_cast<uint8_t>(r));
	}
	inline TexTypeFlags operator&(TexTypeFlags a, TexTypeFlags b)
	{
		return static_cast<TexTypeFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}
	struct TexCreateDesc
	{
		TexCreateDesc()
		{
			m_resouceDesc.Format = DXGI_FORMAT_UNKNOWN;
			m_resouceDesc.Width = 0;
			m_resouceDesc.Height = 0;
			m_resouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			m_resouceDesc.DepthOrArraySize = 1;
			m_resouceDesc.MipLevels = 1;
			m_resouceDesc.SampleDesc.Count = 1;
			m_resouceDesc.SampleDesc.Quality = 0;
			m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			m_resouceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			m_resouceDesc.Alignment = 0;
		}

		D3D12_RESOURCE_DESC m_resouceDesc{};
		TexTypeFlags m_typeFlag = TexTypeFlags::None;
	};

	class DX12TextureResource : public DX12GPUResource
	{
	public:
		DX12TextureResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		DX12TextureResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation);
		DX12TextureResource(DX12TextureResource&& texResource) = default;
		DX12TextureResource(DX12TextureResource& a) = delete;
		DX12TextureResource& operator=(DX12TextureResource& a) = delete;
		DX12TextureResource(const DX12TextureResource& a) = delete;
		DX12TextureResource& operator=(const DX12TextureResource& a) = delete;
		~DX12TextureResource() ;

		DX12DescriptorHeapHandle GetRTVDescriptor() const
		{
			return m_RTVDescriptor;
		}
		DX12DescriptorHeapHandle GetDSVDescriptor() const
		{
			return m_DSVDescriptor;
		}
		DX12DescriptorHeapHandle GetSRVDescriptor() const
		{
			return m_SRVDescriptor;
		}
		DX12DescriptorHeapHandle GetUAVDescriptor() const
		{
			return m_UAVDescriptor;
		}

		void SetRTVDescriptor(DX12DescriptorHeapHandle& handle)
		{
			m_RTVDescriptor = handle;
		}
		void SetSRVDescriptor(DX12DescriptorHeapHandle& handle)
		{
			m_SRVDescriptor = handle;
		}
		void SetDSVDescriptor(DX12DescriptorHeapHandle& handle)
		{
			m_DSVDescriptor = handle;
		}

	private:
		DX12DescriptorHeapHandle m_RTVDescriptor{};
		DX12DescriptorHeapHandle m_DSVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};
}
