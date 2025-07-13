#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12TextureResource : public DX12GPUResource
	{
	public:
		DX12TextureResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		~DX12TextureResource() override;

		DX12DescriptorHeapHandle GetRTVDescriptor() const
		{
			return m_RTVDescriptor;
		}
		DX12DescriptorHeapHandle GetDSVDescriptor()
		{
			return m_DSVDescriptor;
		}
		DX12DescriptorHeapHandle GetSRVDescriptor()
		{
			return m_SRVDescriptor;
		}
		DX12DescriptorHeapHandle GetUAVDescriptor()
		{
			return m_UAVDescriptor;
		}

		void SetRTVDescriptor(DX12DescriptorHeapHandle& handle)
		{
			m_RTVDescriptor = handle;
		}

	private:
		DX12DescriptorHeapHandle m_RTVDescriptor{};
		DX12DescriptorHeapHandle m_DSVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};
}
