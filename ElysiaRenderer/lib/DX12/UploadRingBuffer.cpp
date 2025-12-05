#include "stdafx.h"
#include "UploadRingBuffer.h"

#include "DX12Device.h"
#include "lib/Utility/Helper.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
    UploadRingBuffer::UploadRingBuffer(DX12Device* pDevice, const size_t size, LPCWSTR name) :
        m_size(size)
    {
        Init(pDevice, size, name);
    }

    UploadRingBuffer::~UploadRingBuffer()
    {
        assert(m_pResource && m_pCPUPtr);
        
        m_pResource->Unmap(0, nullptr);
        m_pCPUPtr = nullptr;
    }

    ID3D12Resource* UploadRingBuffer::GetResource() const noexcept
    {
        assert(m_pResource);

        return m_pResource;
    }

    void UploadRingBuffer::Init(DX12Device* pDevice, const size_t size, LPCWSTR name)
    {
        assert(pDevice && pDevice->GetAllocator());
        
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Width = AlignU32(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        resourceDesc.Height = 1;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_GENERIC_READ;
        D3D12MA::ALLOCATION_DESC allocationDesc
        {
            .HeapType = D3D12_HEAP_TYPE_UPLOAD
        };
        CComPtr<D3D12MA::Allocation> pAllocation = nullptr;
        ElysiaHelper::ThrowIfFailed(pDevice->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
            &pAllocation, IID_PPV_ARGS(&m_pResource)));
        
        if(name)
        {
            m_pResource->SetName(name);
        }
        
        ThrowIfFailed(m_pResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pCPUPtr)));
        m_gpuAddress = m_pResource->GetGPUVirtualAddress();
    }
    
    bool UploadRingBuffer::Allocate(size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress,
            UINT8*& outCPUAddress,
            size_t alignment)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const size_t alignedSize = AlignUp(size, alignment);
        const size_t currentOffset = AlignUp(m_head, alignment);
        // 尝试主段分配
        if (currentOffset + alignedSize <= m_size)
        {
            outCPUAddress = m_pCPUPtr + currentOffset;
            outGPUAddress = m_gpuAddress + currentOffset;
            m_head = currentOffset + alignedSize;
            return true;
        }

        // 主段不够 → 尝试绕回到开头（wrap around）
        if (alignedSize <= m_size)
        {
            Reset();  // 会把 m_head 设为 0
            outCPUAddress = m_pCPUPtr;
            outGPUAddress = m_gpuAddress;
            m_head = alignedSize;
            return true;
        }

        return false;
    }

    void UploadRingBuffer::Reset()
    {
        m_head = 0;
    }
    
    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
           UploadRingBuffer* pUploadBuffer,
           size_t totalSize)
    {
        if (totalSize == 0)
        {
            return 0;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
        UINT8* CPUAddress = nullptr;
 
        if(!pUploadBuffer->Allocate(totalSize, GPUAddress, CPUAddress))
        {
            assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
            return 0;
        }
        memset(CPUAddress, 0, totalSize);

        memcpy(CPUAddress, RenderResource::GetInstance().GetCBVFrameVariable(), totalSize);

        return GPUAddress;
    }
}
