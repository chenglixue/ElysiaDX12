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
        DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
        DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState,
                            CComPtr<D3D12MA::Allocation> allocation);
        DX12TextureResource(CComPtr<ID3D12Resource> resource, std::vector<D3D12_RESOURCE_STATES>& usageState,
                            CComPtr<D3D12MA::Allocation> allocation);
        DX12TextureResource(DX12TextureResource&& texResource) = default;
        DX12TextureResource(DX12TextureResource& a) = delete;
        DX12TextureResource& operator=(DX12TextureResource& a) = delete;
        DX12TextureResource(const DX12TextureResource& a) = delete;
        DX12TextureResource& operator=(const DX12TextureResource& a) = delete;
        ~DX12TextureResource();

        void SetUAVCount(const UINT UAVCount)
        {
            m_UAVDescriptors.resize(UAVCount);
            m_UAVResourceHeapIndices.resize(UAVCount);
        }
        void SetSRVCount(const UINT SRVCount)
        {
            m_SRVDescriptors.resize(SRVCount);
            m_SRVResourceHeapIndices.resize(SRVCount);
        }

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
        UINT GetUAVResourceHeapIndex(UINT index) const
        {
            assert(index < m_UAVResourceHeapIndices.size());
            return m_UAVResourceHeapIndices[index];
        }
        UINT GetSRVResourceHeapIndex(UINT index) const
        {
            assert(index < m_SRVResourceHeapIndices.size());
            return m_SRVResourceHeapIndices[index];
        }

        void SetRTVDescriptor(const DX12DescriptorHeapHandle& handle)
        {
            m_RTVDescriptor = handle;
        }
        void SetSRVDescriptor(const DX12DescriptorHeapHandle& handle, UINT index = 0)
        {
            m_SRVDescriptors[index] = handle;
        }
        void SetDSVDescriptor(const DX12DescriptorHeapHandle& handle)
        {
            m_DSVDescriptor = handle;
        }
        void SetUAVDescriptor(const DX12DescriptorHeapHandle& handle, UINT handleIndex = 0)
        {
            m_UAVDescriptors[handleIndex] = handle;
        }

        void SetUAVResourceHeapIndex(UINT index, const UINT UAVResourceHeapIndex)
        {
            assert(index < m_UAVResourceHeapIndices.size());
            m_UAVResourceHeapIndices[index] = UAVResourceHeapIndex;
        }
        void SetSRVResourceHeapIndex(UINT index, const UINT SRVResourceHeapIndex)
        {
            assert(index < m_SRVResourceHeapIndices.size());
            m_SRVResourceHeapIndices[index] = SRVResourceHeapIndex;
        }

    private:
        DX12DescriptorHeapHandle m_RTVDescriptor{};
        DX12DescriptorHeapHandle m_DSVDescriptor{};
        std::vector<DX12DescriptorHeapHandle> m_SRVDescriptors{};
        std::vector<DX12DescriptorHeapHandle> m_UAVDescriptors{};
        std::vector<UINT> m_UAVResourceHeapIndices{};
        std::vector<UINT> m_SRVResourceHeapIndices{};
    };
}