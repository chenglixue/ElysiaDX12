#pragma once

namespace ElysiaRenderer 
{
    class DX12Device;
    class DX12Queue;
    
    class UploadRingBuffer
    {
    public:
        UploadRingBuffer(DX12Device* pDevice, const size_t size, LPCWSTR name = L"");
        ~UploadRingBuffer();

        ID3D12Resource* GetResource() const noexcept;

        void Init(DX12Device* pDevice, const size_t size, LPCWSTR name = L"");
        
        bool Allocate(size_t size,
            D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress, UINT8*& outCPUAddress,
            size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        void Reset();

    private:
        CComPtr<ID3D12Resource>     m_pResource = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS   m_gpuAddress = 0;
        UINT8*                      m_pCPUPtr = nullptr;
        size_t                      m_size = 0;
        size_t                      m_head = 0;             // 当前分配位置（字节偏移）

        DX12Queue*                  m_pQueue = nullptr;
        std::mutex                  m_mutex;
    };
    
    D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstant(
            UploadRingBuffer* pUploadBuffer,
            size_t totalSize,
            UINT8*& CPUAddress);
}


