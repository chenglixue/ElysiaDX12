#pragma once
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"
#include "TextureUtility.h"

namespace ElysiaCore
{
    class DX12TextureResource : public DX12GPUResource
    {
    public:
        DX12TextureResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
        DX12TextureResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState,
                            ComPtr<D3D12MA::Allocation> allocation);
        DX12TextureResource(ComPtr<ID3D12Resource> resource, std::vector<D3D12_RESOURCE_STATES>& usageState,
                            ComPtr<D3D12MA::Allocation> allocation);
        DX12TextureResource(DX12TextureResource&& texResource) = default;
        DX12TextureResource(DX12TextureResource& a) = delete;
        DX12TextureResource& operator=(DX12TextureResource& a) = delete;
        DX12TextureResource(const DX12TextureResource& a) = delete;
        DX12TextureResource& operator=(const DX12TextureResource& a) = delete;
        ~DX12TextureResource();

        DX12DescriptorHeapHandle GetRTVDescriptor() const
        {
            return m_RTVDescriptor;
        }
        DX12DescriptorHeapHandle GetDSVDescriptor() const
        {
            return m_DSVDescriptor;
        }
        DX12DescriptorHeapHandle GetSRVDescriptor(UINT index = 0) const
        {
            return m_SRVDescriptors[index];
        }
        DX12DescriptorHeapHandle GetUAVDescriptor(UINT handleIndex = 0) const
        {
            return m_UAVDescriptors[handleIndex];
        }
        UINT GetUAVHeapIndex(UINT mipmapLevel = 0) const
        {
            assert(mipmapLevel < m_UAVDescriptors.size());
            return m_uavBaseHeapIndex + mipmapLevel;
        }
        UINT GetSRVHeapIndex(UINT mipmapLevel = 0) const
        {
            assert(mipmapLevel < m_SRVDescriptors.size());
            return m_srvBaseHeapIndex + mipmapLevel;
        }

        void SetSRVCount(UINT index)
        {
            m_SRVDescriptors.resize(index);
        }
        void SetUAVCount(UINT index)
        {
            m_UAVDescriptors.resize(index);
        }

        void SetRTVDescriptor(const DX12DescriptorHeapHandle& handle)
        {
            m_RTVDescriptor = handle;
        }
        void SetSRVDescriptor(const DX12DescriptorHeapHandle& handle, UINT index = 0)
        {
            assert(index < m_SRVDescriptors.size());
            m_SRVDescriptors[index] = handle;
        }
        void SetDSVDescriptor(const DX12DescriptorHeapHandle& handle)
        {
            m_DSVDescriptor = handle;
        }
        void SetUAVDescriptor(const DX12DescriptorHeapHandle& handle, UINT handleIndex = 0)
        {
            assert(handleIndex < m_UAVDescriptors.size());
            m_UAVDescriptors[handleIndex] = handle;
        }

        void SetUAVBaseHeapIndex(UINT baseIndex)
        {
            m_uavBaseHeapIndex = baseIndex;
        }
        void SetSRVBaseHeapIndex(UINT baseIndex)
        {
            m_srvBaseHeapIndex = baseIndex;
        }

        UINT GetSubResourceUAVHeapIndex(UINT mipLevel) const
        {
            if (m_uavBaseHeapIndex == INVALID_RESOURCE_TABLE_INDEX)
                return INVALID_RESOURCE_TABLE_INDEX;
            return m_uavBaseHeapIndex + mipLevel;
        }

    private:
        DX12DescriptorHeapHandle m_RTVDescriptor{};
        DX12DescriptorHeapHandle m_DSVDescriptor{};
        std::vector<DX12DescriptorHeapHandle> m_SRVDescriptors{};
        std::vector<DX12DescriptorHeapHandle> m_UAVDescriptors{};
        UINT m_uavBaseHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
        UINT m_srvBaseHeapIndex = INVALID_RESOURCE_TABLE_INDEX;
    };
}