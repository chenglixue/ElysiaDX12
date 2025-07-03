#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12DescriptorHeapHandle
	{
	public:
		DX12DescriptorHeapHandle()
		{
			m_CPUHandle.ptr = NULL;
			m_GPUHandle.ptr = NULL;
			m_heapIndex = 0;
		}
		~DX12DescriptorHeapHandle()
		{
			m_CPUHandle.ptr = NULL;
			m_GPUHandle.ptr = NULL;
			m_heapIndex = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle()
		{
			return m_CPUHandle;
		}
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle()
		{
			return m_GPUHandle;
		}
		uint64_t GetHeapIndex()
		{
			return m_heapIndex;
		}

		void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
		{
			m_CPUHandle = CPUHandle;
		}
		void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle)
		{
			m_GPUHandle = GPUHandle;
		}
		void SetHeapIndex(uint64_t heapIndex)
		{
			m_heapIndex = heapIndex;
		}

		bool IsValid()
		{
			m_CPUHandle.ptr != NULL;
		}
		bool IsReferenceShader()
		{
			m_GPUHandle.ptr != NULL;
		}

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle;
		uint64_t m_heapIndex;
	};
}
