#pragma once
#include <queue>

#include "Programs/Helper.h"
#include "Programs/IManager.h"
#include "Programs/IUpdate.h"
#include "Runtime/Core/BufferUtility.h"

namespace ElysiaModel
{
    struct LoadedModel;
}

namespace ElysiaCore
{
    struct DX12TextureUpload;
    class UploadRingBuffer;
    class DX12UploadContext;
    struct BufferCreationDesc;
}

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;
    using namespace ElysiaCore;

    class BufferManager : public IManager, IUpdate
    {
    public:


    public:
        BufferManager();
        BufferManager(const BufferManager& rhs) = delete;
        BufferManager& operator=(BufferManager& rhs) = delete;
        BufferManager(BufferManager&& rhs) = default;
        ~BufferManager();

        static BufferManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new BufferManager());
                           });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;
        virtual void Update(const ElysiaEngine::FrameContext& context) override;

        D3D12MA::Allocator* GetAllocator() const noexcept;
        UploadRingBuffer* GetUploadRingBuffer() const noexcept;
        BufferHandle GetGlobalVertexBuffer() const noexcept
        {
            return m_globalVertexBuffer;
        }
        BufferHandle GetGlobalIndexBuffer() const noexcept
        {
            return m_globalIndexBuffer;
        }
        D3D12_VERTEX_BUFFER_VIEW& GetGlobalVertexBufferView() noexcept
        {
            return m_globalVertexView;
        }
        D3D12_INDEX_BUFFER_VIEW& GetGlobalIndexBufferView() noexcept
        {
            return m_globalIndexView;
        }

        void SetGlobalVertexBuffer(BufferHandle&& bufferHandle)
        {
            m_globalVertexBuffer = bufferHandle;
        }
        void SetGlobalIndexBuffer(BufferHandle&& bufferHandle)
        {
            m_globalIndexBuffer = bufferHandle;
        }
        void SetGlobalVertexBufferView(D3D12_VERTEX_BUFFER_VIEW&& view)
        {
            m_globalVertexView = view;
        }
        void SetGlobalIndexBufferView(D3D12_INDEX_BUFFER_VIEW&& view)
        {
            m_globalIndexView = view;
        }

        BufferHandle CreateBuffer(const BufferCreationDesc& bufferCreationDesc);
        void DestoryBuffer(const BufferHandle handle);
        void Release(BufferHandle handle);
        void ProcessGarbage(uint64_t currentFrameIndex);

        void UploadBufferData(DX12UploadContext* uploadContext,
                              std::vector<DX12BufferUpload*>& bufferUploads, bool isErase = true);
        void UploadTextureData(DX12UploadContext* uploadContext,
                               std::vector<DX12TextureUpload*>& textureUploads);
        void UploadBufferData(DX12UploadContext* uploadContext,
                              DX12BufferUpload* bufferUpload);

        BufferHandle CreateVertexBuffer(const ElysiaModel::LoadedModel& model);
        BufferHandle CreateIndexBuffer(const ElysiaModel::LoadedModel& model);
        BufferHandle CreateVertexBuffer(const BufferCreationDesc& desc);
        BufferHandle CreateIndexBuffer(const BufferCreationDesc& desc);

    private:
        UINT m_frameID;
        UINT64 m_frameIndex;
        static std::unique_ptr<BufferManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        DX12Device* m_pDevice = nullptr;
        D3D12MA::Allocator* m_pAllocator = nullptr;

        std::mutex m_createMutex;
        std::mutex m_garbageMutex;

        std::vector<BufferHandle> m_bufferPools;
        std::queue<uint32_t> m_freeBufferIndices;
        std::vector<std::pair<uint64_t, BufferHandle>> m_grbageQueue;

        std::unique_ptr<UploadRingBuffer> m_pUploadBuffer;

        BufferHandle m_globalVertexBuffer;
        BufferHandle m_globalIndexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_globalVertexView;
        D3D12_INDEX_BUFFER_VIEW m_globalIndexView;
    };


}