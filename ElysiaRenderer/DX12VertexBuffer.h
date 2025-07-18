#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12VertexBuffer : public DX12GPUResource
	{
	public:
		DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize);
		DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize, 
			D3D12MA::Allocation* allocation);
		~DX12VertexBuffer() override;

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

	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		//DX12DescriptorHeapHandle m_SRVDescriptor;
		void* m_mappedBuffer;
		BufferType m_bufferType = BufferType::Vertex;
	};
}