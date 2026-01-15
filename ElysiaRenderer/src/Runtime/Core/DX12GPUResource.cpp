#include "stdafx.h"
#include "DX12GPUResource.h"

namespace ElysiaCore
{
    DX12GPUResource::DX12GPUResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState) :
        m_state(GPUResourceState::InUse)
    {
        m_resource = resource;
        m_resourceDesc = m_resource->GetDesc();
        m_usageStates.resize(1);
        m_usageStates[0] = usageState;
        m_GPUAddress = 0;
        m_isReady = false;
    }
    DX12GPUResource::DX12GPUResource(CComPtr<ID3D12Resource> resource,
                                     std::vector<D3D12_RESOURCE_STATES>& usageStates) :
        m_state(GPUResourceState::InUse)
    {
        m_resource = resource;
        m_resourceDesc = m_resource->GetDesc();
        m_usageStates = usageStates;
        m_GPUAddress = 0;
        m_isReady = false;
    }
    DX12GPUResource::~DX12GPUResource()
    {
        Destory();
    }

    void DX12GPUResource::Destory()
    {
        if (m_resource)
        {
            m_resource.Release();
            m_resource = nullptr;
        }
        if (m_allocation)
        {
            m_allocation.Release();
            m_allocation = nullptr;
        }
        m_usageStates[0] = D3D12_RESOURCE_STATE_COMMON;
        m_isReady = false;
        m_descriptorHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
    }

    void DX12GPUResource::Reset()
    {
        m_state = GPUResourceState::Free;
    }

}