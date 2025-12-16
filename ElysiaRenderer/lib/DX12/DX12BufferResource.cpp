#include "stdafx.h"
#include "DX12BufferResource.h"

#include "DX12StagingDescriptorHeap.h"
#include "DX12UploadContext.h"


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
		Destory();
	}

	UINT DX12BufferResource::GetIndex() const noexcept
	{
		return m_index;
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

	void DX12BufferResource::SetIndex(UINT index)
	{
		m_index = index;
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
	
	bool DX12BufferResource::ReInit(DX12Device* pDevice, const BufferCreationDesc& bufferCreationDesc)
	{
		if(!m_resource || !m_allocation)
		{
			return false;
		}
		
		UINT numElements = static_cast<UINT>(bufferCreationDesc.stride > 0 ? bufferCreationDesc.size / bufferCreationDesc.stride : 1);
		bool isHostVisible = ((bufferCreationDesc.accessFlags & BufferAccessFlags::HostWritable) == BufferAccessFlags::HostWritable);
		bool isHasCBV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::CBV) == GPUResourceFlags::CBV);
		bool isHasSRV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV);
		bool isHasUAV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::UAV) == GPUResourceFlags::UAV);
		
		if(isHasCBV && m_CBVDescriptor.IsValid())
		{
			m_CBVDescriptor.Reset();
			pDevice->GetSRVStageHeap()->FreeDescriptorHeapHandle(m_CBVDescriptor);
		}
		if(isHasSRV && m_SRVDescriptor.IsValid())
		{
			m_SRVDescriptor.Reset();
			pDevice->GetSRVStageHeap()->FreeDescriptorHeapHandle(m_SRVDescriptor);
		}
		if(isHasUAV && m_UAVDescriptor.IsValid())
		{
			m_UAVDescriptor.Reset();
			pDevice->GetSRVStageHeap()->FreeDescriptorHeapHandle(m_UAVDescriptor);
		}
		m_state = GPUResourceState::InUse;
		
		auto alignSize = AlignU32((UINT)bufferCreationDesc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		if(alignSize > m_resourceDesc.Width)
		{
			return false;
		}
		m_resourceDesc.Width = alignSize;
		m_stride = bufferCreationDesc.stride;
		
		if(bufferCreationDesc.name)
		{
			m_resource->SetName(bufferCreationDesc.name);
		}
		
		m_usageState = isHostVisible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
		pDevice->GetUploadContext()->AddBarrier(*this, m_usageState);
		
		if (isHasCBV)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc{};
			CBVDesc.SizeInBytes = static_cast<UINT>(m_resourceDesc.Width);
			CBVDesc.BufferLocation = m_GPUAddress;

			SetCBVDescriptor(pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());
			pDevice->GetDevice()->CreateConstantBufferView(&CBVDesc, GetCBVDescriptor().GetCPUHandle());
		}

		if (isHasSRV)
		{ 
			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Format = bufferCreationDesc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.isRawAccess ? bufferCreationDesc.size / 4 : numElements);
			SRVDesc.Buffer.StructureByteStride = bufferCreationDesc.stride > 0 ? static_cast<UINT>(GetStride()) : 0;
			SRVDesc.Buffer.Flags = bufferCreationDesc.isRawAccess ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

			SetSRVDescriptor(pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());
			pDevice->GetDevice()->CreateShaderResourceView(GetResource(), &SRVDesc, GetSRVDescriptor().GetCPUHandle());

			SetResourceHeapIndex(pDevice->m_freeReservedDescriptorIndices.back());
			pDevice->m_freeReservedDescriptorIndices.pop_back();

			pDevice->CopyDescriptorFromStageToRenderPass(GetSRVDescriptor(), GetResourceHeapIndex());
		}

		if (isHasUAV)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};

			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			UAVDesc.Format = bufferCreationDesc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			UAVDesc.Buffer.CounterOffsetInBytes = 0;
			UAVDesc.Buffer.FirstElement = 0;
			UAVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.isRawAccess ? bufferCreationDesc.size / 4 : numElements);
			UAVDesc.Buffer.StructureByteStride = bufferCreationDesc.isRawAccess ? 0 : static_cast<UINT>(GetStride());
			UAVDesc.Buffer.Flags = bufferCreationDesc.isRawAccess ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

			SetUAVDescriptor(pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());

			pDevice->GetDevice()->CreateUnorderedAccessView(GetResource(), nullptr, &UAVDesc, GetUAVDescriptor().GetCPUHandle());
		}
		
		return true;
	}
}