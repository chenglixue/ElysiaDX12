#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
	DX12TextureResource::DX12TextureResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState) :
		DX12GPUResource(resource, usageState)
	{

	}

	DX12TextureResource::DX12TextureResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, 
		D3D12MA::Allocation* allocation) :
		DX12GPUResource(resource, usageState)
	{
		m_allocation = allocation;
	}

	DX12TextureResource::~DX12TextureResource()
	{

	}
}