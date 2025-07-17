#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"


namespace ElysiaRenderer
{
	class DX12ConstantBuffer : public DX12GPUResource
	{
	public:
		DX12ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize, 
			DX12DescriptorHeapHandle constantBufferViewHandle);
		~DX12ConstantBuffer() override;

		void SetConstantBufferData(const void* bufferData, uint32_t bufferSize);
		DX12DescriptorHeapHandle GetConstantBufferViewHandle()
		{
			return m_constantBufferViewHandle;
		}

	private:
		void* m_mappedBuffer;
		uint32_t m_bufferSize;
		DX12DescriptorHeapHandle m_constantBufferViewHandle;
	};
}
