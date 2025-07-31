#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	/*class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, bool isVertexBuffer);
		~DX12BufferResource() override;

		void SetMappedData(void* data, size_t dataSize)
		{
			assert(m_mappedResource != nullptr && data != nullptr && dataSize > 0 && dataSize < m_resourceDesc.Width);
			memcpy_s(m_mappedResource, m_resourceDesc.Width, data, dataSize);
		}
		void SetStride(UINT stride)
		{
			m_stride = stride;
		}

		uint8_t* GetMappedResource()
		{
			return m_mappedResource;
		}
		UINT GetStride()
		{
			return m_stride;
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

		void SetCBVDescriptor(DX12DescriptorHeapHandle CBVDescriptor)
		{
			m_CBVDescriptor = CBVDescriptor;
		}
		void SetSRVDescriptor(DX12DescriptorHeapHandle SRVDescriptor)
		{
			m_SRVDescriptor = SRVDescriptor;
		}
		void SetUAVDescriptor(DX12DescriptorHeapHandle UAVDescriptor)
		{
			m_UAVDescriptor = UAVDescriptor;
		}

	private:
		uint8_t* m_mappedResource = nullptr;
		uint32_t m_stride = 0;
		DX12DescriptorHeapHandle m_CBVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};*/

	class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState)
			: DX12GPUResource(resource, usageState)
		{

		}

		~DX12BufferResource() override
		{
		}
	};
}
