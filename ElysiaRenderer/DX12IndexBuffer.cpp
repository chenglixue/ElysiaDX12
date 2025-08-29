#include "DX12IndexBuffer.h"

namespace ElysiaRenderer
{
	DX12IndexBuffer::DX12IndexBuffer(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState,
		DXGI_FORMAT format, uint32_t bufferSize, void* vertexMappedBuffer,
		CComPtr<D3D12MA::Allocation> allocation)
		: DX12BufferResource(resource, usageState)
	{
		m_bufferType = BufferType::Index;

		m_allocation = allocation;
		m_GPUAddress = resource->GetGPUVirtualAddress();

		m_indexBufferView.SizeInBytes = bufferSize;
		m_indexBufferView.Format = format;
		m_indexBufferView.BufferLocation = m_GPUAddress;

		m_mappedBuffer = vertexMappedBuffer;
		m_resource->Map(0, nullptr, &m_mappedBuffer);
	}

	DX12IndexBuffer::~DX12IndexBuffer()
	{
		UnMap();
		Destory();
	}

	void DX12IndexBuffer::SetMappedData(const void* bufferData, uint32_t bufferSize)
	{
		assert(bufferSize <= m_indexBufferView.SizeInBytes);

		memcpy(m_mappedBuffer, bufferData, bufferSize);
	}
}