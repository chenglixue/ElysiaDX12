#pragma once
#include"DX12BufferResource.h"

namespace ElysiaRenderer 
{
    class DX12Device;
    class DX12Queue;
    
    class UploadRingBuffer
    {
    public:
        UploadRingBuffer(DX12Device* pDevice, DX12Queue* qQueue, const size_t size, LPCWSTR name = L"");
        ~UploadRingBuffer();

        uint8_t* GetGPUPtr() const noexcept;

        HRESULT Init(DX12Device* pDevice, const size_t size, LPCWSTR name = L"");
        bool Allocate(size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress, void*& outCPUAddress,
            size_t& outOffset, size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        void WaitGPU(UINT64 fenceValue);
        void Reset();

    private:
        struct Allocation
        {
            UINT64 offset = 0;                            // offset within the ring buffer resource
            UINT64 size = 0;                              // requested size
        };
        
        std::unique_ptr<DX12BufferResource> m_pBuffer = nullptr;
        UINT8* m_pCPUPtr = nullptr;
        size_t m_size = 0;
        size_t m_head = 0;
        D3D12MA::Allocator* m_allocator = nullptr;

        DX12Queue* m_pQueue = nullptr;
        std::mutex m_mutex;
    };
}


