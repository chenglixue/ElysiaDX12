#include "stdafx.h"
#include "UploadRingBuffer.h"

#include "DX12Device.h"
#include "Programs/Helper.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "RenderHelper.h"

namespace ElysiaCore
{
    UploadRingBuffer::UploadRingBuffer(ElysiaCore::DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name) :
        m_size(size)
    {
        Init(pDevice, pAllocator, size, name);
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

    size_t UploadRingBuffer::GetSegmentSize() const noexcept
    {
        return m_segmentSize;
    }

    void UploadRingBuffer::Init(ElysiaCore::DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name)
    {
        assert(pDevice && pAllocator);

        m_totalSize = AlignU32(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        m_segmentSize = m_totalSize / NUM_FRAMES_IN_FLIGHT;
        
        D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_totalSize);
        D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_GENERIC_READ;
        D3D12MA::ALLOCATION_DESC allocationDesc
        {
            .HeapType = D3D12_HEAP_TYPE_UPLOAD
        };
        CComPtr<D3D12MA::Allocation> pAllocation = nullptr;
        ElysiaHelper::ThrowIfFailed(pAllocator->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
            &pAllocation, IID_PPV_ARGS(&m_pResource)));
        
        if(name)
        {
            m_pResource->SetName(name);
        }
        
        ThrowIfFailed(m_pResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pCPUPtr)));
        m_gpuAddress = m_pResource->GetGPUVirtualAddress();
    }
    
    bool UploadRingBuffer::AllocateForFrame(UINT frameID, size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress,
            UINT8*& outCPUAddress,
            size_t alignment)
    {
        assert(frameID < NUM_FRAMES_IN_FLIGHT);
        assert(size > 0);
       
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const size_t alignedSize = AlignUp(size, alignment);
        const size_t baseOffset = m_segmentSize * frameID;

        if (alignedSize > m_segmentSize)
        {
            ThrowRuntimeError("Per-frame upload data exceeds segment size!");
            return false;
        }

        auto& used = m_frameUsed[frameID];
        const size_t currOffset = AlignUp(used, alignment); // 固定从 segment 头部开始
        const size_t finalOffset = baseOffset + currOffset;

        if (currOffset + alignedSize > m_segmentSize)
        {
            ThrowRuntimeError("too small segment of upload ring buffer");
            return false; 
        }

        outCPUAddress = m_pCPUPtr + finalOffset;
        outGPUAddress = m_gpuAddress + finalOffset;

        used = currOffset + alignedSize;

        return true;
    }

    void UploadRingBuffer::Reset(UINT frameID)
    {
        assert(frameID < NUM_FRAMES_IN_FLIGHT);
        m_frameUsed[frameID] = 0;
    }

    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
        DX12Device* pDevice,
        std::function<void (ElysiaRenderer::CBVFrameVariable*) > callBack)
    {
        UINT frameID = pDevice->GetFrameID();
        size_t totalSize = sizeof(ElysiaRenderer::CBVFrameVariable);

        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
        UINT8* CPUAddress = nullptr;
        if(!ElysiaRenderer::BufferManager::GetInstance().GetUploadRingBuffer()->AllocateForFrame(frameID, totalSize, GPUAddress, CPUAddress))
        {
            assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
            return 0;
        }
        auto dst = reinterpret_cast<ElysiaRenderer::CBVFrameVariable*>(CPUAddress);

        callBack(dst);
        
        return GPUAddress;
    }
}
