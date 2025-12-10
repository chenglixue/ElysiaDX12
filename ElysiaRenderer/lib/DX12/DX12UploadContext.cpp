#include "stdafx.h"
#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"
#include"DX12UploadContext.h"
#include "lib/Utility/d3dx12_resource_helpers.h"

#include "Manager/BufferManager.h"

namespace ElysiaRenderer
{
	DX12UploadContext::DX12UploadContext(DX12Device* device,
		std::unique_ptr<DX12BufferResource> bufferUploadHeap,
		std::unique_ptr<DX12BufferResource> textureUploadHeap)
		: DX12Context(device, D3D12_COMMAND_LIST_TYPE_COPY),
		m_bufferUploadHeap(std::move(bufferUploadHeap)),
		m_textureUploadHeap(std::move(textureUploadHeap))
	{

	}

	DX12UploadContext::~DX12UploadContext()
	{
		m_textureUploads.clear();
	}

	DX12BufferResource* DX12UploadContext::GetTexUploadHeap()
	{
		return m_textureUploadHeap.get();
	}
	DX12BufferResource* DX12UploadContext::GetBufferUploadHeap()
	{
		return m_bufferUploadHeap.get();
	}

	void DX12UploadContext::AddTextureToUploads(DX12TextureUpload* textureUpload)
	{
		assert(textureUpload->m_textureDataSize <= m_textureUploadHeap->GetResourceDesc().Width);

		m_textureUploads.emplace_back(std::move(textureUpload));
	}
	void DX12UploadContext::AddBufferToUploads(DX12BufferUpload* bufferUpload)
	{
		assert(bufferUpload->m_bufferDataSize < m_bufferUploadHeap->GetResourceDesc().Width);

		m_bufferUploads.emplace_back(std::move(bufferUpload));
	}

	void DX12UploadContext::ProcessUploads()
	{
		const auto numTextureUploads = static_cast<UINT>(m_textureUploads.size());
		const auto numBufferUploads = static_cast<UINT>(m_bufferUploads.size());
		size_t texUploadHeapOffset = 0;
		size_t bufferUploadHeapOffset = 0;
		UINT numTexsProcessed = 0;
		UINT numBuffersProcessed = 0;

		for (numBuffersProcessed; numBuffersProcessed < numBufferUploads; numBuffersProcessed++)
		{
			auto bufferUpload = m_bufferUploads[numBuffersProcessed];

			if ((bufferUploadHeapOffset + bufferUpload->m_bufferDataSize) > m_bufferUploadHeap->GetResourceDesc().Width)
			{
				break;
			}

			AddBarrier(*bufferUpload->m_buffer, D3D12_RESOURCE_STATE_COPY_DEST);
			memcpy(m_bufferUploadHeap->GetMappedBuffer() + bufferUploadHeapOffset, bufferUpload->m_bufferData.get(), bufferUpload->m_bufferDataSize);

			CopyBufferRegion(*bufferUpload->m_buffer, 0, *m_bufferUploadHeap, bufferUploadHeapOffset, bufferUpload->m_bufferDataSize);

			AddBarrier(*bufferUpload->m_buffer, D3D12_RESOURCE_STATE_COMMON);
			
			bufferUploadHeapOffset += bufferUpload->m_bufferDataSize;
			bufferUploadHeapOffset = AlignUp(bufferUploadHeapOffset, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			m_bufferUploadsInProgress.emplace_back(std::move(bufferUpload));
		}

		for (numTexsProcessed; numTexsProcessed < numTextureUploads; ++numTexsProcessed)
		{
			auto currUpload = m_textureUploads[numTexsProcessed];
			if ((texUploadHeapOffset + currUpload->m_textureDataSize) > m_textureUploadHeap->GetResourceDesc().Width)
			{
				break;
			}

			memcpy(m_textureUploadHeap->GetMappedBuffer() + texUploadHeapOffset, currUpload->m_pTextureData.get(), currUpload->m_textureDataSize);
			UpdateSubresources(m_commandList, 
				currUpload->m_textureBuffer->GetResource(), m_textureUploadHeap->GetResource(), texUploadHeapOffset,
				0, currUpload->m_numSubResources, currUpload->m_subResourceLayouts.data());
			// CopyTextureRegion(*currUpload->m_textureBuffer, *m_textureUploadHeap, texUploadHeapOffset,
			// 	currUpload->m_subResourceLayouts, currUpload->m_numSubResources);

			texUploadHeapOffset += currUpload->m_textureDataSize;
			texUploadHeapOffset = AlignU64(texUploadHeapOffset, 512);

			m_textureUploadsInProgress.emplace_back(std::move(currUpload));
		}

		if (numBuffersProcessed > 0)
		{
			m_bufferUploads.erase(m_bufferUploads.begin(), m_bufferUploads.begin() + numBuffersProcessed);
		}

		if (numTexsProcessed > 0)
		{
			m_textureUploads.erase(m_textureUploads.begin(), m_textureUploads.begin() + numTexsProcessed);
		}
	}

	void DX12UploadContext::ResolveProcessedUploads()
	{
		for (auto& bufferUploadInProgress : m_bufferUploadsInProgress)
		{
			bufferUploadInProgress->onComplete(bufferUploadInProgress);
			bufferUploadInProgress->m_buffer->SetIsReady(true);
		}
		for (auto& textureUploadInProgress : m_textureUploadsInProgress)
		{
			textureUploadInProgress->m_textureBuffer->SetIsReady(true);
		}

		m_bufferUploadsInProgress.clear();
		m_textureUploadsInProgress.clear();
	}
}