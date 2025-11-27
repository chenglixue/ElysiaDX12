#include "stdafx.h"
#include "DX12GPUResource.h"

namespace ElysiaRenderer
{
	DX12GPUResource::DX12GPUResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState)
	{
		m_resource = resource;
		m_resourceDesc = m_resource->GetDesc();
		m_usageState = usageState;
		m_GPUAddress = 0;
		m_isReady = false;
	}
	DX12GPUResource::~DX12GPUResource()
	{
		Destory();
	}
}