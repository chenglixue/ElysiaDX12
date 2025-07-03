#pragma once
#include "stdafx.h"
#include "GPUResource.h"

#define D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT

namespace ElysiaRenderer
{
	class DX12ConstantBuffer : public DX12GPUResource
	{
	public:
		DX12ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize);
		~DX12ConstantBuffer() override;

		void SetConstantBufferData(const void* bufferData, uint32_t bufferSize);

	private:
		void* m_mappedBuffer;
		uint32_t m_bufferSize;

	};
}
