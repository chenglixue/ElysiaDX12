#include "stdafx.h"
#include "../public/UploadRingBuffer.h"

#include "../public/DX12Device.h"
#include "Programs/public/Helper.h"
#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/CBVParameter.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaCore
{
    UploadRingBuffer::UploadRingBuffer(ElysiaCore::DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name) :
        m_size(size)
    {
        Init(pDevice, pAllocator, size, name);
    }

    UploadRingBuffer::~UploadRingBuffer()
    {
        for (UINT frameID = 0; frameID < ElysiaHelper::NUM_FRAMES_IN_FLIGHT; ++frameID)
        {
            for (Heap& heap : m_extraHeaps[frameID])
                DestroyHeap(heap);
            for (Heap& heap : m_standaloneHeaps[frameID])
                DestroyHeap(heap);
        }

        if (m_pResource && m_pCPUPtr)
        {
            m_pResource->Unmap(0, nullptr);
            m_pCPUPtr = nullptr;
        }
    }

    ID3D12Resource* UploadRingBuffer::GetResource() const noexcept
    {
        assert(m_pResource);

        return m_pResource.Get();
    }

    size_t UploadRingBuffer::GetSegmentSize() const noexcept
    {
        return m_segmentSize;
    }

    void UploadRingBuffer::Init(ElysiaCore::DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name)
    {
        assert(pDevice && pAllocator);

        m_pAllocator = pAllocator;
        m_totalSize = AlignU32(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        m_segmentSize = m_totalSize / ElysiaHelper::NUM_FRAMES_IN_FLIGHT;

        Heap primary;
        CreateHeap(m_totalSize, primary, name);
        m_pAllocation = std::move(primary.allocation);
        m_pResource = std::move(primary.resource);
        m_pCPUPtr = primary.cpu;
        m_gpuAddress = primary.gpu;
        primary.cpu = nullptr;
    }

    bool UploadRingBuffer::CreateHeap(size_t capacity, Heap& out, const wchar_t* name)
    {
        capacity = AlignUp(capacity, static_cast<size_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

        D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(capacity);
        D3D12MA::ALLOCATION_DESC allocationDesc
        {
            .HeapType = D3D12_HEAP_TYPE_UPLOAD
        };
        ThrowIfFailed(m_pAllocator->CreateResource(&allocationDesc,
                                                   &resourceDesc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr,
                                                   &out.allocation,
                                                   IID_PPV_ARGS(&out.resource)));
        if (name && name[0] != L'\0')
            out.resource->SetName(name);

        ThrowIfFailed(out.resource->Map(0, nullptr, reinterpret_cast<void**>(&out.cpu)));
        out.gpu = out.resource->GetGPUVirtualAddress();
        out.capacity = capacity;
        out.used = 0;
        return true;
    }

    void UploadRingBuffer::DestroyHeap(Heap& heap)
    {
        if (heap.resource && heap.cpu)
            heap.resource->Unmap(0, nullptr);
        heap.cpu = nullptr;
        heap.gpu = 0;
        heap.resource.Reset();
        heap.allocation.Reset();
        heap.capacity = 0;
        heap.used = 0;
    }

    bool UploadRingBuffer::TryPlace(Heap& heap, size_t size, size_t alignment, UploadAllocation& out)
    {
        const size_t alignedSize = AlignUp(size, alignment);
        const size_t offset = AlignUp(heap.used, alignment);
        if (alignedSize > heap.capacity || offset + alignedSize > heap.capacity)
            return false;

        out.resource = heap.resource.Get();
        out.cpuAddress = heap.cpu + offset;
        out.gpuAddress = heap.gpu + offset;
        heap.used = offset + alignedSize;
        return true;
    }

    bool UploadRingBuffer::TryPlacePrimary(UINT frameID, size_t size, size_t alignment, UploadAllocation& out)
    {
        const size_t alignedSize = AlignUp(size, alignment);
        if (alignedSize > m_segmentSize)
            return false;

        size_t& used = m_frameUsed[frameID];
        const size_t offset = AlignUp(used, alignment);
        if (offset + alignedSize > m_segmentSize)
            return false;

        const size_t finalOffset = m_segmentSize * frameID + offset;
        out.resource = m_pResource.Get();
        out.cpuAddress = m_pCPUPtr + finalOffset;
        out.gpuAddress = m_gpuAddress + finalOffset;
        used = offset + alignedSize;
        return true;
    }
    
    bool UploadRingBuffer::AllocateForFrame(UINT frameID, size_t size, UploadAllocation& out, size_t alignment)
    {
        assert(frameID < ElysiaHelper::NUM_FRAMES_IN_FLIGHT);
        if (size == 0)
            return false;

        std::lock_guard<std::mutex> lock(m_mutex);

        if (TryPlacePrimary(frameID, size, alignment, out))
            return true;

        for (Heap& heap : m_extraHeaps[frameID])
        {
            if (TryPlace(heap, size, alignment, out))
                return true;
        }

        const size_t alignedSize = AlignUp(size, alignment);
        if (alignedSize > m_segmentSize)
        {
            Heap standalone;
            CreateHeap(alignedSize, standalone, L"Upload Standalone");
            const bool placed = TryPlace(standalone, size, alignment, out);
            m_standaloneHeaps[frameID].push_back(std::move(standalone));
            return placed;
        }

        Heap extra;
        CreateHeap(m_segmentSize, extra, L"Upload Segment");
        const bool placed = TryPlace(extra, size, alignment, out);
        m_extraHeaps[frameID].push_back(std::move(extra));
        return placed;
    }

    void UploadRingBuffer::Reset(UINT frameID)
    {
        assert(frameID < ElysiaHelper::NUM_FRAMES_IN_FLIGHT);
        std::lock_guard<std::mutex> lock(m_mutex);

        m_frameUsed[frameID] = 0;
        for (Heap& heap : m_extraHeaps[frameID])
            heap.used = 0;
        for (Heap& heap : m_standaloneHeaps[frameID])
            DestroyHeap(heap);
        m_standaloneHeaps[frameID].clear();
    }

    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
        DX12Device* pDevice,
        std::function<void (ElysiaRenderer::CBVFrameVariable*) > callBack)
    {
        UINT frameID = pDevice->GetFrameID();
        size_t totalSize = sizeof(ElysiaRenderer::CBVFrameVariable);

        UploadAllocation allocation;
        if(!ElysiaRenderer::BufferManager::GetInstance().GetUploadRingBuffer()->AllocateForFrame(frameID, totalSize, allocation))
        {
            assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
            return 0;
        }
        auto dst = reinterpret_cast<ElysiaRenderer::CBVFrameVariable*>(allocation.cpuAddress);

        callBack(dst);
        
        return allocation.gpuAddress;
    }
}
