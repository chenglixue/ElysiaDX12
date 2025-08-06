#include "DX12RenderPassDescriptorHeap.h"

namespace ElysiaRenderer
{
	DX12RenderPassDescriptorHeap::DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT reservedCount, UINT userCount)
		: DX12DescriptorHeap(device, heapType, reservedCount + userCount, true)
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

	DX12DescriptorHeapHandle DX12RenderPassDescriptorHeap::AllocateRenderPassDescriptorBlock(UINT count)
	{
		std::lock_guard<std::mutex> lockGuard(m_usageMutex);

		UINT newHandleID = 0;

		UINT blockEnd = m_currDescriptorIndex + count;
		if (blockEnd <= m_maxDescriptors)
		{
			newHandleID = m_currDescriptorIndex;
			m_currDescriptorIndex = blockEnd;
		}
		else
		{
			ElysiaHelper::AssertError("Ran out of render pass descriptor heap handles, need to increase heap size");
		}

		DX12DescriptorHeapHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descriptorHeapCPUStart;
		cpuHandle.ptr += static_cast<size_t>(newHandleID) * m_descriptorSize;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_descriptorHeapGPUStart;
		gpuHandle.ptr += static_cast<size_t>(newHandleID) * m_descriptorSize;

		newHandle.SetHeapIndex(newHandleID);
		newHandle.SetCPUHandle(cpuHandle);
		newHandle.SetGPUHandle(gpuHandle);

		return newHandle;
	}
}
	 