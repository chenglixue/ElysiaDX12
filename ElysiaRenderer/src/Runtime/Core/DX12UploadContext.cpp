#include "stdafx.h"
#include"DX12UploadContext.h"

#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"
#include "ThirdParty/d3dx12_resource_helpers.h"

#include "Runtime/RenderCore/BufferManager.h"
#include "BufferUtility.h"
#include "TextureUtility.h"

namespace ElysiaCore
{
	DX12UploadContext::DX12UploadContext(DX12Device* device)
		: DX12Context(device, D3D12_COMMAND_LIST_TYPE_COPY)
	{

	}

	DX12UploadContext::~DX12UploadContext()
	{
		m_textureUploads.clear();
	}

	void DX12UploadContext::AddTextureToUploads(DX12TextureUpload* textureUpload)
	{
		m_textureUploads.emplace_back(std::move(textureUpload));
	}
	void DX12UploadContext::AddBufferToUploads(DX12BufferUpload* bufferUpload)
	{
		m_bufferUploads.emplace_back(std::move(bufferUpload));
	}
	
	void DX12UploadContext::AddBufferProcess(DX12BufferUpload* bufferProcessed)
	{
		m_bufferUploadsInProgress.emplace_back(bufferProcessed);
	}
	void DX12UploadContext::AddTextureProcess(DX12TextureUpload* textureProcessed)
	{
		m_textureUploadsInProgress.emplace_back(textureProcessed);
	}
	
	void DX12UploadContext::ProcessUploads()
	{
		BufferManager::GetInstance().UploadBufferData(this, m_bufferUploads);
		BufferManager::GetInstance().UploadTextureData(this, m_textureUploads);
	}

	void DX12UploadContext::ResolveProcessedUploads()
	{
		for (auto& bufferUploadInProgress : m_bufferUploadsInProgress)
		{
			bufferUploadInProgress->buffer->SetIsReady(true);
		}
		for (auto& textureUploadInProgress : m_textureUploadsInProgress)
		{
			textureUploadInProgress->pTextureBuffer->SetIsReady(true);
		}

		m_bufferUploadsInProgress.clear();
		m_textureUploadsInProgress.clear();
	}
}