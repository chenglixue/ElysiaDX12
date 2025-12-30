#pragma once
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

#include "DX12Device.h"

namespace ElysiaCore
{
	class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
		DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, CComPtr<D3D12MA::Allocation> allocation);

		~DX12BufferResource();

		UINT GetIndex() const noexcept;
		UINT GetStride() const noexcept;
		DX12DescriptorHeapHandle GetCBVDescriptor() const noexcept;
		DX12DescriptorHeapHandle GetSRVDescriptor() const noexcept;
		DX12DescriptorHeapHandle GetUAVDescriptor() const noexcept;
		uint8_t* GetMappedBuffer() const noexcept;

		void SetIndex(UINT index);
		void SetStride(UINT stride);
		void SetCBVDescriptor(const DX12DescriptorHeapHandle& CBVDescriptor);
		void SetSRVDescriptor(const DX12DescriptorHeapHandle& SRVDescriptor);
		void SetUAVDescriptor(const DX12DescriptorHeapHandle& UAVDescriptor);
		void SetMappedData(const void* bufferData, size_t bufferSize);
		
		bool ReInit(DX12Device* pDevice, const BufferCreationDesc& bufferCreationDesc);

		uint8_t* m_mappedBuffer = nullptr;
	protected:
		bool m_isDirty = false;
		size_t m_stride = 0;
		DX12DescriptorHeapHandle m_CBVDescriptor;
		DX12DescriptorHeapHandle m_SRVDescriptor;
		DX12DescriptorHeapHandle m_UAVDescriptor;

		// index of buffer pool
		UINT m_index;
	};
}
