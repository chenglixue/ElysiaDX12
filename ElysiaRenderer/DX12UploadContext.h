#pragma once
#include "stdafx.h"
#include "DX12Context.h"
#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"

namespace ElysiaRenderer
{
	extern class DX12Device;
	class DX12UploadContext : public DX12Context
	{
	public:
		DX12UploadContext(DX12Device* device, 
			std::unique_ptr<DX12BufferResource> bufferUploadHeap,
			std::unique_ptr<DX12BufferResource> textureUploadHeap);
		~DX12UploadContext() override;

		DX12BufferResource* GetTexUploadHeap()
		{
			return m_textureUploadHeap.get();
		}
		DX12BufferResource* GetBufferUploadHeap()
		{
			return m_bufferUploadHeap.get();
		}

		void AddTextureToUploads(std::unique_ptr<DX12TextureUpload> textureUpload)
		{
			assert(textureUpload->m_textureDataSize <= m_textureUploadHeap->GetResourceDesc().Width);

			m_textureUploads.emplace_back(std::move(textureUpload));
		}
		void AddBufferToUploads(std::unique_ptr<DX12BufferUpload> bufferUpload)
		{
			assert(bufferUpload->m_bufferDataSize < m_bufferUploadHeap->GetResourceDesc().Width);

			m_bufferUploads.emplace_back(std::move(bufferUpload));
		}
		void ProcessUploads();
		void ResolveProcessedUploads();

	private:
		std::vector<std::unique_ptr<DX12BufferUpload>> m_bufferUploads;
		std::vector<std::unique_ptr<DX12TextureUpload>> m_textureUploads;
		std::vector<DX12TextureResource*> m_textureUploadsInProgress;
		std::vector<DX12BufferResource*> m_bufferUploadsInProgress;

		std::unique_ptr<DX12BufferResource> m_textureUploadHeap = nullptr;
		std::unique_ptr<DX12BufferResource> m_bufferUploadHeap = nullptr;
	};
}