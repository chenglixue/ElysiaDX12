#include "DX12TextureBuffer.h"
#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
	DX12TextureBuffer::DX12TextureBuffer(std::unique_ptr<DX12TextureResource> texResource, size_t mipLevels, size_t arraySize)
		: DX12BufferResource(texResource->GetResource(), texResource->GetUsageState())
	{
		m_tex = std::move(texResource);
		m_numSubResources = static_cast<UINT>(mipLevels * arraySize);
		m_allocation = m_tex->GetAllocation();
	}

	/*DX12TextureBuffer::DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation)
		: DX12BufferResource(resource, usageState)
	{
		m_allocation = allocation;
		m_GPUAddress = resource->GetGPUVirtualAddress();
	}*/

	DX12TextureBuffer::~DX12TextureBuffer()
	{
		
	}

	DX12TextureUploadBuffer::DX12TextureUploadBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation)
		:	DX12BufferResource(resource, usageState)
	{
		resource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedBuffer));
	}

	DX12TextureUploadBuffer::~DX12TextureUploadBuffer()
	{
		Unmap();
	}
}