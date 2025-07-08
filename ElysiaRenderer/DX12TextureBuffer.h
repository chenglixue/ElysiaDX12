#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12TextureBuffer : public DX12GPUResource
	{
	public:
		DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DX12DescriptorHeapHandle heapHandle);
		//~DX12TextureBuffer();

		DX12DescriptorHeapHandle GetDescriptorHeapHandle()
		{
			return m_RTVDescriptorHeapHandle;
		}

	private:
		DX12DescriptorHeapHandle m_RTVDescriptorHeapHandle{};
	};
}