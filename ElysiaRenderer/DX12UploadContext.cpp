#include"DX12UploadContext.h"

namespace ElysiaRenderer
{
	DX12UploadContext::DX12UploadContext(DX12Device* device,
		//std::unique_ptr<DX12VertexBuffer> vertexUploadHeap,
		std::unique_ptr<DX12TextureUploadBuffer> textureUploadHeap)
		: DX12Context(device, D3D12_COMMAND_LIST_TYPE_COPY),
		//m_vertexUploadHeap(std::move(vertexUploadHeap)),
		m_textureUploadHeap(std::move(textureUploadHeap))
	{

	}

	DX12UploadContext::~DX12UploadContext()
	{
		assert(m_textureUploadHeap == nullptr);
	}

	void DX12UploadContext::ProcessUploads()
	{
		const auto numTextureUploads = static_cast<UINT>(m_textureUploads.size());
		size_t texUploadHeapOffset = 0;
		UINT numTexsProcessed = 0;

		for (numTexsProcessed; numTexsProcessed < numTextureUploads; ++numTexsProcessed)
		{
			auto& currUpload = *m_textureUploads[numTexsProcessed];
			if ((texUploadHeapOffset + currUpload.GetTextureDataSize()) > m_textureUploadHeap->GetResourceDesc().Width)
			{
				break;
			}

			memcpy(m_textureUploadHeap->GetMappedBuffer() + texUploadHeapOffset, currUpload.GetTexData().get(), currUpload.GetTextureDataSize());
			CopyTextureRegion(*currUpload.GetDefaultHeap(), *m_textureUploadHeap, texUploadHeapOffset,
				currUpload.GetSubResourceLayouts(), currUpload.GetNumSubResources());

			texUploadHeapOffset += currUpload.GetTextureDataSize();
			texUploadHeapOffset = ElysiaHelper::AlignU64(texUploadHeapOffset, 512);
		}

		if (numTexsProcessed > 0)
		{
			m_textureUploads.erase(m_textureUploads.begin(), m_textureUploads.begin() + numTexsProcessed);
		}
	}
}