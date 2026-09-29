#pragma once
#include "Programs/public/Helper.h"

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
    struct UploadAllocation
    {
        ID3D12Resource* resource = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
        UINT8* cpuAddress = nullptr;
    };

    class UploadRingBuffer
    {
    public:
        UploadRingBuffer(DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name = L"");
        ~UploadRingBuffer();

        ID3D12Resource* GetResource() const noexcept;
        size_t GetSegmentSize() const noexcept;

        void Init(DX12Device* pDevice, D3D12MA::Allocator* pAllocator, const size_t size, LPCWSTR name = L"");
        
        bool AllocateForFrame(UINT frameID, size_t size, UploadAllocation& out,
            size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        void Reset(UINT frameID);
    private:
        struct Heap
        {
            ComPtr<D3D12MA::Allocation> allocation;
            ComPtr<ID3D12Resource> resource;
            UINT8* cpu = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
            size_t capacity = 0;
            size_t used = 0;
        };

        bool CreateHeap(size_t capacity, Heap& out, const wchar_t* name);
        void DestroyHeap(Heap& heap);
        bool TryPlace(Heap& heap, size_t size, size_t alignment, UploadAllocation& out);
        bool TryPlacePrimary(UINT frameID, size_t size, size_t alignment, UploadAllocation& out);

        D3D12MA::Allocator*         m_pAllocator = nullptr;
        ComPtr<D3D12MA::Allocation> m_pAllocation;
        ComPtr<ID3D12Resource>      m_pResource = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS   m_gpuAddress = 0;
        UINT8*                      m_pCPUPtr = nullptr;
        size_t                      m_size = 0;

        std::mutex                  m_mutex;
        size_t                      m_totalSize = 0;
        size_t                      m_segmentSize = 0;
        std::array<size_t, ElysiaHelper::NUM_FRAMES_IN_FLIGHT> m_frameUsed{};
        std::array<std::vector<Heap>, ElysiaHelper::NUM_FRAMES_IN_FLIGHT> m_extraHeaps;
        std::array<std::vector<Heap>, ElysiaHelper::NUM_FRAMES_IN_FLIGHT> m_standaloneHeaps;
    };
    
    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
        DX12Device* pDevice,
        std::function<void (ElysiaRenderer::CBVFrameVariable*) > callBack);
}


