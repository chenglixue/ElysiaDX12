#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	DX12BufferResource::DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState) :
		DX12GPUResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();
	}
	DX12BufferResource::DX12BufferResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, bool isVertexBuffer) : 
		DX12GPUResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();

	}

	DX12BufferResource::~DX12BufferResource()
	{

	}
}