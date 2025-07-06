#include "DX12GPUResource.h"

namespace ElysiaRenderer
{
	DX12GPUResource::DX12GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState)
	{
		m_resource = resource;
		m_usageState = usageState;
		m_GPUAddress = 0;
		m_isReady = false;
	}
	DX12GPUResource::~DX12GPUResource()
	{
		m_resource->Release();
		m_resource = nullptr;
	}
}