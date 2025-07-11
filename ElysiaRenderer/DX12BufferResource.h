#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		~DX12BufferResource() override;

		void SetMappedData(void* data, size_t dataSize)
		{
			assert(m_mappedResource != nullptr && data != nullptr && dataSize > 0 && dataSize < m_resourceDesc.Width);
			memcpy_s(m_mappedResource, m_resourceDesc.Width, data, dataSize);
		}

		DX12DescriptorHeapHandle GetCBVDescriptor()
		{
			return m_CBVDescriptor;
		}
		DX12DescriptorHeapHandle GetSRVDescriptor()
		{
			return m_SRVDescriptor;
		}
		DX12DescriptorHeapHandle GetUAVDescriptor()
		{
			return m_UAVDescriptor;
		}

	private:
		uint8_t* m_mappedResource = nullptr;
		uint32_t m_stride = 0;
		DX12DescriptorHeapHandle m_CBVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};
}
