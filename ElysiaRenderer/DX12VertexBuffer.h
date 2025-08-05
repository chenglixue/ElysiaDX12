#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	struct VertexBufferCreationDesc
	{
		UINT m_size = 0;
		UINT m_stride = 0;
		BufferTypeFlags bufferTypeFlags = BufferTypeFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
	};

	class DX12VertexBuffer : public DX12BufferResource
	{
	public:
		DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize);
		DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize, 
			D3D12MA::Allocation* allocation);
		~DX12VertexBuffer();

		D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView()
		{
			return m_vertexBufferView;
		}
		/*DX12DescriptorHeapHandle& GetSRVDescriptor()
		{
			return m_SRVDescriptor;
		}*/
		void* GetMappedBuffer()
		{
			return m_mappedBuffer;
		}

		/*void SetSRVDescriptor(DX12DescriptorHeapHandle&& SRVDescriptor)
		{
			m_SRVDescriptor = SRVDescriptor;
		}*/
		void SetMappedData(const void* bufferData, uint32_t bufferSize);

		void Unmap()
		{
			if (m_mappedBuffer != nullptr)
			{
				m_resource->Unmap(0, nullptr);
			}
		}

	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		//DX12DescriptorHeapHandle m_SRVDescriptor;
		void* m_mappedBuffer;
		BufferType m_bufferType = BufferType::Vertex;
	};
}