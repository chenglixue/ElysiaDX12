#pragma once
#include "Programs/Helper.h"
#include "BufferUtility.h"
#include "ThirdParty/D3D12MemoryAllocator/D3D12MemAlloc.h"

namespace ElysiaCore
{
    using namespace ElysiaHelper;

    class DX12GPUResource
    {
    public:
        DX12GPUResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
        DX12GPUResource(ComPtr<ID3D12Resource> resource, std::vector<D3D12_RESOURCE_STATES>& usageState);
        ~DX12GPUResource();

        GPUResourceType GetBufferType() const noexcept
        {
            return m_bufferType;
        }
        ComPtr<ID3D12Resource>& GetResource()
        {
            return m_resource;
        }
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const noexcept
        {
            return m_GPUAddress;
        }
        D3D12_RESOURCE_STATES GetUsageState(UINT index = 0) const noexcept
        {
            return m_usageStates[index];
        }
        D3D12_RESOURCE_DESC GetResourceDesc() const noexcept
        {
            return m_resourceDesc;
        }
        ComPtr<D3D12MA::Allocation> GetAllocation() const noexcept
        {
            return m_allocation;
        }

        /// <summary>
        /// index of SRV Resource in SRV Descriptor heap
        /// </summary>
        /// <returns></returns>
        const UINT& GetResourceHeapIndex() const noexcept
        {
            return m_descriptorHeapIndex;
        }
        bool IsFree() const noexcept
        {
            return m_state;
        }

        void SetUsageState(D3D12_RESOURCE_STATES usageState, UINT index = 0)
        {
            m_usageStates[index] = usageState;
        }
        void SetResourceDesc(D3D12_RESOURCE_DESC resourceDesc)
        {
            m_resourceDesc = resourceDesc;
        }
        void SetGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS GPUAddress)
        {
            m_GPUAddress = GPUAddress;
        }
        void SetAllocation(ComPtr<D3D12MA::Allocation> allocation)
        {
            m_allocation = allocation;
        }
        void SetResourceHeapIndex(UINT descriptorHeapIndex)
        {
            m_descriptorHeapIndex = descriptorHeapIndex;
        }

        bool GetIsReady() const noexcept
        {
            return m_isReady;
        }
        void SetIsReady(bool isReady)
        {
            m_isReady = isReady;
        }

        virtual void Destory();
        void Reset();

    protected:
        ComPtr<ID3D12Resource> m_resource = nullptr;
        ComPtr<D3D12MA::Allocation> m_allocation = nullptr;
        D3D12_RESOURCE_DESC m_resourceDesc = {};
        D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress = 0;
        // a resource must be in COMMON state before being used on a COPY queue
        std::vector<D3D12_RESOURCE_STATES> m_usageStates;
        bool m_isReady = false;
        UINT m_descriptorHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
        GPUResourceType m_bufferType = GPUResourceType::None;
        GPUResourceState m_state = GPUResourceState::NoInit;
    };
}