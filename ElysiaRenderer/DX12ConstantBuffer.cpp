#include"DX12ConstantBuffer.h"

namespace ElysiaRenderer
{
	DX12ConstantBuffer::DX12ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t bufferSize, 
		DX12DescriptorHeapHandle constantBufferViewHandle)
		: DX12BufferResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_bufferSize = bufferSize;

		m_constantBufferViewHandle = constantBufferViewHandle;

		m_mappedBuffer = nullptr;
		m_resource->Map(0, nullptr, &m_mappedBuffer);
	}

	DX12ConstantBuffer::~DX12ConstantBuffer()
	{
		m_resource->Unmap(0, nullptr);
		ElysiaHelper::SafeRelease(m_resource);
		ElysiaHelper::SafeRelease(m_allocation);
	}

	void DX12ConstantBuffer::SetConstantBufferData(const void* bufferData, uint32_t bufferSize)
	{
		assert(bufferSize <= m_bufferSize);
		memcpy(m_mappedBuffer, bufferData, bufferSize);
	}
}