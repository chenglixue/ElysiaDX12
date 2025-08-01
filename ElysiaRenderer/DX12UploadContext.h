#pragma once
#include "stdafx.h"
#include "DX12Context.h"
#include "DX12BufferResource.h"
#include "DX12VertexBuffer.h"
#include "DX12TextureBuffer.h"
#include "DX12TextureResource.h"
#include "DX12ConstantBuffer.h"

namespace ElysiaRenderer
{
	extern class DX12Device;
	class DX12UploadContext : public DX12Context
	{
	public:
		DX12UploadContext(DX12Device* device, 
			std::unique_ptr<DX12TextureUploadBuffer> textureUploadHeap);
		~DX12UploadContext() override;

		DX12TextureUploadBuffer* GetTexUploadHeap()
		{
			return m_textureUploadHeap.get();
		}

		void AddTextureBufferUpload(std::unique_ptr<DX12TextureBuffer> textureUpload)
		{
			assert(textureUpload->GetTextureDataSize() <= m_textureUploadHeap->GetResourceDesc().Width);

			m_textureUploads.push_back(std::move(textureUpload));
		}
		void ProcessUploads();

	private:
		//std::vector<std::unique_ptr<DX12VertexBuffer>> m_vertexBufferUploads;
		// these not upload heap.their members have default heap, need upload data from m_textureUploadHeap to members's default heap
		std::vector<std::unique_ptr<DX12ConstantBuffer>> m_constantBufferUploads;
		std::vector<std::unique_ptr<DX12TextureBuffer>> m_textureUploads;

		//std::unique_ptr<DX12VertexBuffer> m_vertexUploadHeap;
		std::unique_ptr<DX12TextureUploadBuffer> m_textureUploadHeap;
	};
}