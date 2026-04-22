#pragma once
#include "Programs/Helper.h"

namespace ElysiaCore
{
    class DX12Device;
    class DX12Queue;
}

namespace ElysiaRenderer
{
    struct CBVFrameVariable;
}

namespace ElysiaCore 
{
    class UploadRingBuffer
    {
    public:
        UploadRingBuffer(DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name = L"");
        ~UploadRingBuffer();

        ID3D12Resource* GetResource() const noexcept;
        size_t GetSegmentSize() const noexcept;

        void Init(DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name = L"");
        
        bool AllocateForFrame(UINT frameID, size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress, UINT8*& outCPUAddress,
            size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        void Reset(UINT frameID);
    private:
        ComPtr<ID3D12Resource>     m_pResource = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS   m_gpuAddress = 0;
        UINT8*                      m_pCPUPtr = nullptr;
        size_t                      m_size = 0;
        size_t                      m_head = 0;             // 当前分配位置（字节偏移）

        DX12Queue*                  m_pQueue = nullptr;
        std::mutex                  m_mutex;
        size_t                      m_totalSize;
        size_t                      m_segmentSize;
        std::array<size_t, ElysiaHelper::NUM_FRAMES_IN_FLIGHT> m_frameUsed;
    };
    
    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
        DX12Device* pDevice,
        std::function<void (ElysiaRenderer::CBVFrameVariable*) > callBack);
}


