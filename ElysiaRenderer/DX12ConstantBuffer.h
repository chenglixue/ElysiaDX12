#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"


namespace ElysiaRenderer
{
	struct ConstantBufferCreationDesc
	{
		UINT m_size = 0;
		BufferTypeFlags bufferTypeFlags = BufferTypeFlags::CBV;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
	};

	class DX12ConstantBuffer : public DX12BufferResource
	{
	public:
		DX12ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize, 
			DX12DescriptorHeapHandle constantBufferViewHandle, D3D12MA::Allocation* allocator);
		DX12ConstantBuffer(DX12ConstantBuffer&& a) = default;
		DX12ConstantBuffer(DX12ConstantBuffer& a) = delete;
		DX12ConstantBuffer& operator=(DX12ConstantBuffer& a) = delete;
		DX12ConstantBuffer(const DX12ConstantBuffer& a) = delete;
		DX12ConstantBuffer& operator=(const DX12ConstantBuffer& a) = delete;
		~DX12ConstantBuffer();

		void SetMappedData(const void* bufferData, uint32_t bufferSize);
		DX12DescriptorHeapHandle GetConstantBufferViewHandle()
		{
			return m_constantBufferViewHandle;
		}

	private:
		void* m_mappedBuffer;
		uint32_t m_bufferSize;
		DX12DescriptorHeapHandle m_constantBufferViewHandle;
		BufferType m_bufferType = BufferType::Vertex;
	};
}
