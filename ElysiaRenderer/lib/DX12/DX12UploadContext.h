#pragma once
#include "DX12Context.h"
#include "../Utility/TextureUtility.h"
#include "../Utility/BufferUtility.h"

namespace ElysiaRenderer
{
	class DX12Device;

	class DX12UploadContext : public DX12Context
	{
	public:
		DX12UploadContext(DX12Device* device);
		~DX12UploadContext() override;

		DX12BufferResource* GetTexUploadHeap();
		DX12BufferResource* GetBufferUploadHeap();

		void AddTextureToUploads(DX12TextureUpload* textureUpload);
		void AddBufferToUploads(DX12BufferUpload* bufferUpload);
		
		void AddBufferProcess(DX12BufferUpload* bufferProcessed);
		void AddTextureProcess(DX12TextureUpload* textureProcessed);

		void ProcessUploads();
		void ResolveProcessedUploads();

	private:
		std::vector<DX12BufferUpload*> m_bufferUploads;
		std::vector<DX12TextureUpload*> m_textureUploads;
		std::vector<DX12TextureUpload*> m_textureUploadsInProgress;
		std::vector<DX12BufferUpload*> m_bufferUploadsInProgress;
	};
}