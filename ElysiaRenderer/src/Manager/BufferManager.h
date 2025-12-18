#pragma once
#include <queue>

#include "lib/Utility/Helper.h"

#include "IManager.h"
#include "IUpdate.h"
#include "DX12/DX12MeshRender.h"
#include "DX12/UploadRingBuffer.h"
#include "lib/Utility/RenderTexture.h"
#include "lib/Model/LoadedModel.h"


namespace ElysiaRenderer
{
	class DX12Device;
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

		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;
		virtual void Update() override;

		D3D12MA::Allocator* GetAllocator() const noexcept;
		UploadRingBuffer* GetUploadRingBuffer() const noexcept;

		BufferHandle CreateBuffer(const BufferCreationDesc& bufferCreationDesc);
		void DestoryBuffer(const BufferHandle handle);
		void Release(BufferHandle handle);
		void ProcessGarbage(uint64_t currentFrameIndex);

		void UploadBufferData(DX12UploadContext* uploadContext, std::vector<DX12BufferUpload*>& bufferUploads);
		void UploadTextureData(DX12UploadContext* uploadContext, std::vector<DX12TextureUpload*>& textureUploads);

		BufferHandle CreateVertexBuffer(const LoadedModel& model);
		BufferHandle CreateIndexBuffer(const LoadedModel& model);

		BufferHandle GetVertexBuffer() const noexcept;
		BufferHandle GetIndexBuffer() const noexcept;
		const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const noexcept;
		const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const noexcept;

		void AddVertexBuffer(BufferCreationDesc desc);
		void AddIndexBuffer(BufferCreationDesc desc);
		void SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& view);
		void SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& view);
		
	private:
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

		BufferHandle m_pVertexBuffer = nullptr;
		BufferHandle m_pIndexBuffer = nullptr;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
	};

	
}