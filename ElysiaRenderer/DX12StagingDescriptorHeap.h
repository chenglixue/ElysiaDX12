#pragma once
#include "stdafx.h"
#include "DX12DescriptorHeap.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12StagingDescriptorHeap : public DX12DescriptorHeap
	{
	public:

		DX12StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor);
		~DX12StagingDescriptorHeap() final;

		DX12DescriptorHeapHandle NewDescriptorHeapHandle();
		void FreeDescriptorHeapHandle(DX12DescriptorHeapHandle heapHandle);

	private:
		std::vector<UINT> m_freeDescriptors;
		UINT m_currDescriptorIndex;
		UINT m_activeHandleCount;
		std::mutex m_usageMutex;
	};
}
