#include"DX12ConstantBuffer.h"

namespace ElysiaRenderer
{
	DX12ConstantBuffer::DX12ConstantBuffer(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize,
		DX12DescriptorHeapHandle constantBufferViewHandle, CComPtr<D3D12MA::Allocation> allocator)
		: DX12BufferResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_bufferSize = bufferSize;

		m_constantBufferViewHandle = constantBufferViewHandle;

		m_mappedBuffer = nullptr;
		m_resource->Map(0, nullptr, &m_mappedBuffer);
		m_allocation = allocator;
	}

	DX12ConstantBuffer::~DX12ConstantBuffer()
	{
		m_resource->Unmap(0, nullptr);
		Destory();
	}

	void DX12ConstantBuffer::SetMappedData(const void* bufferData, uint32_t bufferSize)
	{
		assert(m_mappedBuffer != nullptr && bufferData != nullptr && bufferSize > 0 && bufferSize <= m_resourceDesc.Width);

		memcpy_s(m_mappedBuffer, m_resourceDesc.Width, bufferData, bufferSize);
	}
}