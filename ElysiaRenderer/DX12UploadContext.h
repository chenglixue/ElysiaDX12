#pragma once
#include "DX12Context.h"

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

		DX12BufferResource* GetTexUploadHeap();
		DX12BufferResource* GetBufferUploadHeap();

		void AddTextureToUploads(std::unique_ptr<DX12TextureUpload> textureUpload);
		void AddBufferToUploads(std::unique_ptr<DX12BufferUpload> bufferUpload);

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