#include "DX12RenderPassDescriptorHeap.h"

namespace ElysiaRenderer
{
	DX12RenderPassDescriptorHeap::DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor)
		: DX12DescriptorHeap(device, heapType, numDescriptor, true)
	{
		m_currDescriptorIndex = 0;
	}

	DX12RenderPassDescriptorHeap::~DX12RenderPassDescriptorHeap()
	{
		m_currDescriptorIndex = 0;
	}

	void DX12RenderPassDescriptorHeap::Reset()
	{
		m_currDescriptorIndex = 0;
	}

	DX12DescriptorHeapHandle DX12RenderPassDescriptorHeap::GetHeapHandleBlock(UINT count)
	{
		UINT newHandleID = 0;
		UINT blockEnd = m_currDescriptorIndex + count;

		if (blockEnd < m_maxDescriptors)
		{
			newHandleID = m_currDescriptorIndex;
			m_currDescriptorIndex = blockEnd;
		}
		else
		{
			ElysiaHelper::ThrowRuntimeError("Ran out of render pass descriptor heap handles, need to increase heap size.");
		}

		DX12DescriptorHeapHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * m_descriptorSize;
		newHandle.SetCPUHandle(cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_descriptorHeapGPUStart;
		gpuHandle.ptr += newHandleID * m_descriptorSize;
		newHandle.SetGPUHandle(gpuHandle);

		newHandle.SetHeapIndex(newHandleID);

		return newHandle;
	}
}
	 