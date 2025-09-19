#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	struct IndexBufferCreateDesc
	{
		DXGI_FORMAT m_format = DXGI_FORMAT_R16_UINT;
		UINT m_bufferSize = 0;
		void* m_vertexMappedBuffer = nullptr;

		GPUResourceFlags bufferTypeFlags = GPUResourceFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
	};

	class DX12IndexBuffer : public DX12BufferResource
	{
	public:
		DX12IndexBuffer(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, DXGI_FORMAT format, uint32_t bufferSize, void* vertexMappedBuffer,
			CComPtr<D3D12MA::Allocation> allocation);
		~DX12IndexBuffer();

		void SetMappedData(const void* bufferData, uint32_t bufferSize);
		void UnMap()
		{
			if (m_mappedBuffer)
			{
				m_resource->Unmap(0, nullptr);
			}
		}

		D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()
		{
			return m_indexBufferView;
		}
		void* GetMappedBuffer()
		{
			return m_mappedBuffer;
		}

	private:
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
		void* m_mappedBuffer;
		GPUResourceType m_bufferType = GPUResourceType::Vertex;
	};
}