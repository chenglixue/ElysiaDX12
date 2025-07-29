#include "DX12TextureBuffer.h"
#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
	DX12TextureBuffer::DX12TextureBuffer(DX12TextureResource* texResource, size_t mipLevels, size_t arraySize)
		: DX12GPUResource(texResource->GetResource(), texResource->GetUsageState())
	{
		m_numSubResources = static_cast<UINT>(mipLevels * arraySize);
		m_resourceDesc = texResource->GetResourceDesc();
		m_allocation = texResource->GetAllocation();
	}

	DX12TextureBuffer::DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation)
		: DX12GPUResource(resource, usageState)
	{
		m_allocation = allocation;
		m_GPUAddress = resource->GetGPUVirtualAddress();
	}

	DX12TextureBuffer::~DX12TextureBuffer()
	{
		delete m_tex;
		m_tex = nullptr;
	}
}