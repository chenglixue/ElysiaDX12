#pragma once
#include <queue>

#include "Programs/Helper.h"

#include "Programs/IManager.h"
#include "Programs/IUpdate.h"
#include "Runtime/Core/UploadRingBuffer.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/Model/LoadedModel.h"

namespace ElysiaRenderer
{
	struct BufferCreationDesc;
}

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	using namespace ElysiaModel;

	class BufferManager : public IManager, IUpdate
	{
	public:
		
		
	public:
		BufferManager() = default;
		BufferManager(const BufferManager& rhs) = delete;
		BufferManager& operator=(BufferManager& rhs) = delete;
		BufferManager(BufferManager&& rhs) = default;
		~BufferManager();

		static BufferManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new BufferManager());
				});

			return *m_instance;
		}

		virtual void Init(ElysiaCore::DX12Device* pDevice) override;
		virtual void Destory() override;
		virtual void Update(const ElysiaEngine::FrameContext& context) override;

		D3D12MA::Allocator* GetAllocator() const noexcept;
		UploadRingBuffer* GetUploadRingBuffer() const noexcept;

		BufferHandle CreateBuffer(const ElysiaCore::BufferCreationDesc& bufferCreationDesc);
		void DestoryBuffer(const BufferHandle handle);
		void Release(BufferHandle handle);
		void ProcessGarbage(uint64_t currentFrameIndex);

		void UploadBufferData(DX12UploadContext* uploadContext, std::vector<DX12BufferUpload*>& bufferUploads);
		void UploadTextureData(DX12UploadContext* uploadContext, std::vector<DX12TextureUpload*>& textureUploads);

		BufferHandle CreateVertexBuffer(const LoadedModel& model);
		BufferHandle CreateIndexBuffer(const LoadedModel& model);

	private:
		UINT m_frameID;
		UINT64 m_frameIndex;
		static std::unique_ptr<BufferManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		ElysiaCore::DX12Device* m_pDevice = nullptr;
		D3D12MA::Allocator* m_pAllocator = nullptr;
		
		std::mutex m_createMutex;
		std::mutex m_garbageMutex;
		
		std::vector<BufferHandle> m_bufferPools;
		std::queue<uint32_t> m_freeBufferIndices;
		std::vector<std::pair<uint64_t, BufferHandle>> m_grbageQueue;

		std::unique_ptr<ElysiaCore::UploadRingBuffer> m_pUploadBuffer;
	};

	
}