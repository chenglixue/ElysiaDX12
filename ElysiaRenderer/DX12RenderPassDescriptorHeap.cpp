#include "DX12RenderPassDescriptorHeap.h"

namespace ElysiaRenderer
{
	DX12RenderPassDescriptorHeap::DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT reservedCount)
		: DX12DescriptorHeap(device, heapType, reservedCount, true)
	{
		m_currDescriptorIndex = reservedCount;
		m_reservedHandleCount = reservedCount;
	}

	DX12RenderPassDescriptorHeap::~DX12RenderPassDescriptorHeap()
	{
		m_currDescriptorIndex = 0;
		m_reservedHandleCount = 0;
	}

	void DX12RenderPassDescriptorHeap::Reset()
	{
		m_currDescriptorIndex = m_reservedHandleCount;
	}

	DX12DescriptorHeapHandle DX12RenderPassDescriptorHeap::GetReservedDescriptor(UINT index)
	{
		assert(index < m_reservedHandleCount);

		auto CPUHandle = m_descriptorHeapCPUStart;
		auto GPUHandle = m_descriptorHeapGPUStart;
		CPUHandle.ptr += static_cast<size_t>(index) * m_descriptorSize;
		GPUHandle.ptr += static_cast<size_t>(index) * m_descriptorSize;

		DX12DescriptorHeapHandle newHandle = DX12DescriptorHeapHandle();
		newHandle.SetHeapIndex(index);
		newHandle.SetCPUHandle(CPUHandle);
		newHandle.SetGPUHandle(GPUHandle);

		return newHandle;
	}
}
	 