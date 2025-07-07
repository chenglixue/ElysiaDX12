#include "DX12StagingDescriptorHeap.h"

namespace ElysiaRenderer
{
	DX12StagingDescriptorHeap::DX12StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor) :
		DX12DescriptorHeap(device, heapType, numDescriptor, false)
	{
		m_freeDescriptors = std::vector<UINT>();
		m_activeHandleCount = 0;
		m_currDescriptorIndex = 0;
	}
	DX12StagingDescriptorHeap::~DX12StagingDescriptorHeap()
	{
		if (m_activeHandleCount > 0)
		{
			ElysiaHelper::ThrowRuntimeError("There were active handles when the descriptor heap was destroyed. Look for leaks.");
		}

		m_freeDescriptors.clear();
	}

	DX12DescriptorHeapHandle DX12StagingDescriptorHeap::NewDescriptorHeapHandle()
	{
		UINT newHandleID = 0;

		if (m_currDescriptorIndex < m_maxDescriptors)
		{
			newHandleID = m_currDescriptorIndex;
			m_currDescriptorIndex++;
		}
		else if (m_freeDescriptors.size() > 0)
		{
			newHandleID = m_freeDescriptors.back();
			m_freeDescriptors.pop_back();
		}
		else
		{
			ElysiaHelper::ThrowRuntimeError("Ran out of dynamic descriptor heap handles, need to increase heap size.");
		}

		DX12DescriptorHeapHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * m_descriptorSize;
		newHandle.SetCPUHandle(cpuHandle);
		newHandle.SetHeapIndex(newHandleID);
		m_activeHandleCount++;

		return newHandle;
	}

	void DX12StagingDescriptorHeap::FreeDescriptorHeapHandle(DX12DescriptorHeapHandle heapHandle)
	{
		m_freeDescriptors.push_back(heapHandle.GetHeapIndex());

		if (m_activeHandleCount == 0)
		{
			ElysiaHelper::ThrowRuntimeError("Freeing heap handles when there should be none left");
		}

		m_activeHandleCount--;
	}
}