#pragma once
#include "stdafx.h"
#include "GPUResource.h"

namespace ElysiaRenderer
{
	class DX12ConstantBuffer : public DX12GPUResource
	{
	public:
		DX12ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize, 
			DescriptorHeapHandle);
		~DX12ConstantBuffer() override;

	private:

	};
}
