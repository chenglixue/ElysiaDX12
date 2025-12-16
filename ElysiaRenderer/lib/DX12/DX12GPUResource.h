#pragma once
#include "../Utility/Helper.h"
#include "../Utility/BufferUtility.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12GPUResource
	{
	public:
		DX12GPUResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
		~DX12GPUResource();

		GPUResourceType GetBufferType() const noexcept
		{
			return m_bufferType;
		}
		CComPtr<ID3D12Resource>& GetResource()
		{
			return m_resource;
		}
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const noexcept
		{
			return m_GPUAddress;
		}
		D3D12_RESOURCE_STATES GetUsageState() const noexcept
		{
			return m_usageState;
		}
		D3D12_RESOURCE_DESC GetResourceDesc() const noexcept
		{
			return m_resourceDesc;
		}
		CComPtr<D3D12MA::Allocation> GetAllocation() const noexcept
		{
			return m_allocation;
		}

		/// <summary>
		/// index of SRV Resource in SRV Descriptor heap
		/// </summary>
		/// <returns></returns>
		const UINT& GetResourceHeapIndex() const noexcept
		{
			return m_descriptorHeapIndex;
		}
		bool IsFree() const noexcept {return m_state;}

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

		bool GetIsReady() const noexcept
		{
			return m_isReady;
		}
		void SetIsReady(bool isReady)
		{
			m_isReady = isReady;
		}

		virtual void Destory();
		void Reset();

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
		GPUResourceState m_state = GPUResourceState::NoInit;
	};
}
