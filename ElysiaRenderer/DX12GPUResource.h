#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
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

		void SetUsageState(D3D12_RESOURCE_STATES usageState)
		{
			m_usageState = usageState;
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
		ID3D12Resource* m_resource;
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress;
		D3D12_RESOURCE_STATES m_usageState;
		bool m_isReady;
	};
}