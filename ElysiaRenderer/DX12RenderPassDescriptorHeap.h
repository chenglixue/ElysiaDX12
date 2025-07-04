#pragma once
#include "stdafx.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12DescriptorHeap.h"

namespace ElysiaRenderer
{
	class DX12RenderPassDescriptorHeap : DX12DescriptorHeap
	{
	public:
		DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor);
		~DX12RenderPassDescriptorHeap() final;

		void Reset();
		DX12DescriptorHeapHandle GetHeapHandleBlock(UINT count);

	private:
		UINT m_currDescriptorIndex;
	};
}