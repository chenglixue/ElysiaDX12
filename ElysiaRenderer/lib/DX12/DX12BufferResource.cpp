#include "stdafx.h"
#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	DX12BufferResource::DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState)
		: DX12GPUResource(resource, usageState),
		m_isDirty(false),
		m_mappedBuffer(nullptr)
	{
		
	}
	DX12BufferResource::DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, CComPtr<D3D12MA::Allocation> allocation)
		: DX12GPUResource(resource, usageState),
		m_isDirty(false),
		m_mappedBuffer(nullptr)
	{
		m_bufferType = GPUResourceType::Buffer;

		m_allocation = allocation;
		m_GPUAddress = m_resource->GetGPUVirtualAddress();
	}

	DX12BufferResource::~DX12BufferResource()
	{
		if (m_resource != nullptr)
		{
			m_resource->Unmap(0, nullptr);
		}
	}

	float DX12BufferResource::GetStride() const noexcept
	{
		return static_cast<float>(m_stride);
	}
	DX12DescriptorHeapHandle DX12BufferResource::GetCBVDescriptor() const noexcept
	{
		return m_CBVDescriptor;
	}
	DX12DescriptorHeapHandle DX12BufferResource::GetSRVDescriptor() const noexcept
	{
		return m_SRVDescriptor;
	}
	DX12DescriptorHeapHandle DX12BufferResource::GetUAVDescriptor() const noexcept
	{
		return m_UAVDescriptor;
	}
	uint8_t* DX12BufferResource::GetMappedBuffer() const noexcept
	{
		return m_mappedBuffer;
	}

	void DX12BufferResource::SetStride(float stride)
	{
		m_stride = static_cast<size_t>(stride);
	}
	void DX12BufferResource::SetCBVDescriptor(const DX12DescriptorHeapHandle& CBVDescriptor)
	{
		m_CBVDescriptor = CBVDescriptor;
	}
	void DX12BufferResource::SetSRVDescriptor(const DX12DescriptorHeapHandle& SRVDescriptor)
	{
		m_SRVDescriptor = SRVDescriptor;
	}
	void DX12BufferResource::SetUAVDescriptor(const DX12DescriptorHeapHandle& UAVDescriptor)
	{
		m_UAVDescriptor = UAVDescriptor;
	}
	void DX12BufferResource::SetMappedData(const void* bufferData, size_t bufferSize)
	{
		assert(m_mappedBuffer != nullptr && bufferData != nullptr && bufferSize > 0 && bufferSize <= m_resourceDesc.Width);
		memcpy_s(m_mappedBuffer, m_resourceDesc.Width, bufferData, bufferSize);
	}
	
}