#include "stdafx.h"
#include "UploadRingBuffer.h"

#include "DX12Device.h"
#include "lib/Utility/Helper.h"
#include "DX12Queue.h"

namespace ElysiaRenderer
{
    UploadRingBuffer::UploadRingBuffer(DX12Device* pDevice, DX12Queue* qQueue, const size_t size, LPCWSTR name) :
        m_size(size),
        m_pQueue(qQueue)
    {
        Init(pDevice, size, name);
    }

    UploadRingBuffer::~UploadRingBuffer()
    {
        m_pBuffer.reset();
    }

    uint8_t* UploadRingBuffer::GetGPUPtr() const noexcept
    {
        assert(m_pBuffer && m_pBuffer->GetMappedBuffer());

        return m_pBuffer->m_mappedBuffer;
    }


    HRESULT UploadRingBuffer::Init(DX12Device* pDevice, const size_t size, LPCWSTR name)
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
        CComPtr<ID3D12Resource> pResource = nullptr;
        ElysiaHelper::ThrowIfFailed(pDevice->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
            &pAllocation, IID_PPV_ARGS(&pResource)));

        m_pBuffer = std::make_unique<DX12BufferResource>(pResource, usageState, pAllocation);

        m_pBuffer->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&m_pBuffer->m_mappedBuffer));
    }
    bool UploadRingBuffer::Allocate(size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress,
            void*& outCPUAddress,
            size_t& outOffset,
            size_t alignment)
    {
        size_t alignedOffset = AlignUp(m_head, alignment);
        if (alignedOffset + size > m_size)
        {
            Reset();
            return false;
        }

        outOffset = alignedOffset;
        outCPUAddress = m_pCPUPtr + alignedOffset;
        outGPUAddress = m_pBuffer->GetGPUAddress() + alignedOffset;

        m_head = alignedOffset + size;

        return true;
    }

    void UploadRingBuffer::WaitGPU(UINT64 fenceValue)
    {
        if (m_pQueue->IsFenceCompleted(fenceValue))
        {
            m_pQueue->WaitForFenceCPUBlocking(fenceValue);
        }
    }
    void UploadRingBuffer::Reset()
    {
        m_head = 0;
    }
}
