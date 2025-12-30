#pragma once
#include "DX12Context.h"

namespace ElysiaCore
{
	class DX12Device;
	struct DX12BufferUpload;
	struct DX12TextureUpload;
}

namespace ElysiaCore
{
	class DX12UploadContext : public DX12Context
	{
	public:
		DX12UploadContext(DX12Device* device);
		~DX12UploadContext() override;

		void AddTextureToUploads(DX12TextureUpload* textureUpload);
		void AddBufferToUploads(DX12BufferUpload* bufferUpload);
		
		void AddBufferProcess(DX12BufferUpload* bufferProcessed);
		void AddTextureProcess(DX12TextureUpload* textureProcessed);

		void ProcessUploads();
		void ResolveProcessedUploads();

		bool HasWork();

	private:
		std::vector<DX12BufferUpload*> m_bufferUploads;
		std::vector<DX12TextureUpload*> m_textureUploads;
		std::vector<DX12TextureUpload*> m_textureUploadsInProgress;
		std::vector<DX12BufferUpload*> m_bufferUploadsInProgress;
	};
}