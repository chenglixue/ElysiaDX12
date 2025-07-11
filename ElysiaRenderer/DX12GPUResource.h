#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;

	class DX12GPUResource
	{
	public:
		DX12GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		virtual ~DX12GPUResource();

		ID3D12Resource* GetResource()
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

		void SetUsageState(D3D12_RESOURCE_STATES usageState)
		{
			m_usageState = usageState;
		}
		void SetResourceDesc(D3D12_RESOURCE_DESC resourceDesc)
		{
			m_resourceDesc = resourceDesc;
		}

		bool GetIsReady()
		{
			return m_isReady;
		}
		void SetIsReady(bool isReady)
		{
			m_isReady = isReady;
		}

	protected:
		ID3D12Resource* m_resource = nullptr;
		D3D12_RESOURCE_DESC m_resourceDesc = {};
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = 0;
		// a resource must be in COMMON state before being used on a COPY queue
		D3D12_RESOURCE_STATES m_usageState = D3D12_RESOURCE_STATE_COMMON;
		bool m_isReady = false;
		UINT m_descriptorHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
	};
}