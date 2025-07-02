#include "DX12VertexBuffer.h"

namespace ElysiaRenderer
{
	DX12VertexBuffer::DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize)
		: DX12GPUResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_vertexBufferView.BufferLocation = m_GPUAddress;
		m_vertexBufferView.SizeInBytes = bufferSize;
		m_vertexBufferView.StrideInBytes = vertexStride;
	}

	DX12VertexBuffer::~DX12VertexBuffer()
	{
		m_resource->Release();
		m_resource = nullptr;
	}
}