#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	enum class BufferType : uint8_t
	{
		 None = 0,
		 Vertex = 1,
		 Constant = 2,
		 Texture = 3
	};

	enum class BufferTypeFlags : uint8_t
	{
		None = 0,
		CBV = 1,
		SRV = 2,
		UAV = 3
	};

	enum class BufferAccessFlags : uint8_t
	{
		GPUOnly = 0,
		HostWritable = 1
	};

	class DX12GPUResource
	{
	public:
		DX12GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		virtual ~DX12GPUResource();

		BufferType GetBufferType()
		{
			return m_bufferType;
		}
		ID3D12Resource*& GetResource()
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
		D3D12_RESOURCE_DESC& GetResourceDesc()
		{
			return m_resourceDesc;
		}
		D3D12MA::Allocation*& GetAllocation()
		{
			return m_allocation;
		}
		UINT GetResourceHeapIndex()
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
		void SetAllocation(D3D12MA::Allocation* allocation)
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

	protected:
		ID3D12Resource* m_resource = nullptr;
		D3D12MA::Allocation* m_allocation = nullptr;
		D3D12_RESOURCE_DESC m_resourceDesc = {};
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = 0;
		// a resource must be in COMMON state before being used on a COPY queue
		D3D12_RESOURCE_STATES m_usageState = D3D12_RESOURCE_STATE_COMMON;
		bool m_isReady = false;
		UINT m_descriptorHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
		BufferType m_bufferType = BufferType::None;
	};
}