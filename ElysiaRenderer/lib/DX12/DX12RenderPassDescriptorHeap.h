#pragma once
#include "DX12DescriptorHeap.h"

namespace ElysiaRenderer
{
	class DX12DescriptorHeapHandle;

	class DX12RenderPassDescriptorHeap final : public DX12DescriptorHeap
	{
	public:
		DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT reservedCount, UINT userCount);
		~DX12RenderPassDescriptorHeap() final;

		void Reset();
		DX12DescriptorHeapHandle GetReservedDescriptor(UINT index);
		DX12DescriptorHeapHandle AllocateRenderPassDescriptorBlock(UINT count);

	private:
		UINT m_currDescriptorIndex;
		UINT m_reservedHandleCount;
		std::mutex m_usageMutex;
	};
}