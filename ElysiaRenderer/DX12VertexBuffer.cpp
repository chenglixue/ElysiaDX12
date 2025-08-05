#include "DX12VertexBuffer.h"

namespace ElysiaRenderer
{
	DX12VertexBuffer::DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize)
		: DX12BufferResource(resource, usageState)
	{
		m_bufferType = BufferType::Vertex;
		//m_SRVDescriptor = {};
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_vertexBufferView.BufferLocation = m_GPUAddress;
		m_vertexBufferView.SizeInBytes = bufferSize;
		m_vertexBufferView.StrideInBytes = vertexStride;

		m_mappedBuffer = nullptr;
		m_resource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedBuffer));
	}

	DX12VertexBuffer::DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize,
		D3D12MA::Allocation* allocation)
		: DX12BufferResource(resource, usageState)
	{
		m_bufferType = BufferType::Vertex;
		//m_SRVDescriptor = {};
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_vertexBufferView.BufferLocation = m_GPUAddress;
		m_vertexBufferView.SizeInBytes = bufferSize;
		m_vertexBufferView.StrideInBytes = vertexStride;

		m_mappedBuffer = nullptr;
		m_resource->Map(0, nullptr, &m_mappedBuffer);

		m_allocation = allocation;
	}

	DX12VertexBuffer::~DX12VertexBuffer()
	{
		m_resource->Unmap(0, nullptr);

		/*ElysiaHelper::SafeRelease(m_resource);
		ElysiaHelper::SafeRelease(m_allocation);*/
	}

	void DX12VertexBuffer::SetMappedData(const void* bufferData, uint32_t bufferSize)
	{
		assert(bufferSize <= m_vertexBufferView.SizeInBytes);
		memcpy(m_mappedBuffer, bufferData, bufferSize);
	}
}