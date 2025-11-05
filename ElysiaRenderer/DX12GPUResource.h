#pragma once
#include "Helper.h"
#include "BufferUtility.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12GPUResource
	{
	public:
		DX12GPUResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
		~DX12GPUResource();

		GPUResourceType GetBufferType()
		{
			return m_bufferType;
		}
		CComPtr<ID3D12Resource> GetResource()
		{
			return m_resource;
		}
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress()
		{
			return m_GPUAddress;
		}
		D3D12_RESOURCE_STATES GetUsageState()
		{
			return m_usageState;
		}
		D3D12_RESOURCE_DESC GetResourceDesc()
		{
			return m_resourceDesc;
		}
		CComPtr<D3D12MA::Allocation> GetAllocation()
		{
			return m_allocation;
		}

		/// <summary>
		/// index of SRV Resource in SRV Descriptor heap
		/// </summary>
		/// <returns></returns>
		const UINT& GetResourceHeapIndex()
		{
			return m_descriptorHeapIndex;
		}

		void SetUsageState(D3D12_RESOURCE_STATES usageState)
		{
			m_usageState = usageState;
		}
		void SetResourceDesc(D3D12_RESOURCE_DESC resourceDesc)
		{
			m_resourceDesc = resourceDesc;
		}
		void SetGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS GPUAddress)
		{
			m_GPUAddress = GPUAddress;
		}
		void SetAllocation(CComPtr<D3D12MA::Allocation> allocation)
		{
			m_allocation = allocation;
		}
		void SetResourceHeapIndex(UINT descriptorHeapIndex)
		{
			m_descriptorHeapIndex = descriptorHeapIndex;
		}

		bool GetIsReady()
		{
			return m_isReady;
		}
		void SetIsReady(bool isReady)
		{
			m_isReady = isReady;
		}

		virtual void Destory()
		{
			//ElysiaHelper::SafeRelease(m_allocation);
			//ElysiaHelper::SafeRelease(m_resource);
			//m_GPUAddress = 0;
		}

	protected:
		CComPtr<ID3D12Resource> m_resource = nullptr;
		CComPtr<D3D12MA::Allocation> m_allocation = nullptr;
		D3D12_RESOURCE_DESC m_resourceDesc = {};
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = 0;
		// a resource must be in COMMON state before being used on a COPY queue
		D3D12_RESOURCE_STATES m_usageState = D3D12_RESOURCE_STATE_COMMON;
		bool m_isReady = false;
		UINT m_descriptorHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
		GPUResourceType m_bufferType = GPUResourceType::None;
	};
}