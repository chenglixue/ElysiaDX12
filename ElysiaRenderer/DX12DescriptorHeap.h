#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12DescriptorHeap
	{
	public:
		DX12DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor, bool isReferenceShader);
		virtual ~DX12DescriptorHeap();

		ID3D12DescriptorHeap* GetDescriptorHeap()
		{
			return m_descriptorHeap;
		}
		D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType()
		{
			return m_heapType;
		}
		UINT GetDescriptorSingleSize()
		{
			return m_descriptorSize;
		}
		UINT GetDescriptorMaxNum()
		{
			m_maxDescriptors;
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetHeapStartCPU()
		{
			return m_descriptorHeapCPUStart;
		}
		D3D12_GPU_DESCRIPTOR_HANDLE GetHeapStartGPU()
		{
			return m_descriptorHeapGPUStart;
		}

	protected:
		ID3D12DescriptorHeap* m_descriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		UINT m_maxDescriptors;
		UINT m_descriptorSize;
		D3D12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE m_descriptorHeapGPUStart;
		bool m_isReferenceShader;
	};
}
