#include "stdafx.h"
#include "DX12TextureBuffer.h"

namespace ElysiaCore
{
    DX12TextureResource::DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState) :
        DX12GPUResource(resource, usageState)
    {
        m_bufferType = GPUResourceType::Texture;
        m_GPUAddress = resource->GetGPUVirtualAddress();
    }

    DX12TextureResource::DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState,
                                             CComPtr<D3D12MA::Allocation> allocation) :
        DX12GPUResource(resource, usageState)
    {
        m_bufferType = GPUResourceType::Texture;
        m_GPUAddress = resource->GetGPUVirtualAddress();

        m_allocation = allocation;
    }

    DX12TextureResource::DX12TextureResource(CComPtr<ID3D12Resource> resource,
                                             std::vector<D3D12_RESOURCE_STATES>& usageStates,
                                             CComPtr<D3D12MA::Allocation> allocation) :
        DX12GPUResource(resource, usageStates)
    {
        m_bufferType = GPUResourceType::Texture;
        m_GPUAddress = resource->GetGPUVirtualAddress();

        m_allocation = allocation;
    }

    DX12TextureResource::~DX12TextureResource()
    {
    }
}