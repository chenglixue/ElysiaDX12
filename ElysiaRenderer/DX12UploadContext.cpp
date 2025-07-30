#include"DX12UploadContext.h"

namespace ElysiaRenderer
{
	DX12UploadContext::DX12UploadContext(DX12Device* device,
		//std::unique_ptr<DX12VertexBuffer> vertexUploadHeap,
		std::unique_ptr<DX12TextureResource> textureUploadHeap)
		: DX12Context(device, D3D12_COMMAND_LIST_TYPE_COPY),
		//m_vertexUploadHeap(std::move(vertexUploadHeap)),
		m_textureUploadHeap(std::move(textureUploadHeap))
	{

	}

	DX12UploadContext::~DX12UploadContext()
	{
		m_constantBufferUploads.clear();
		m_textureDefaults.clear();
		m_textureUploadHeap.release();
	}

	void DX12UploadContext::ProcessUploads()
	{
		const auto numTextureUploads = static_cast<UINT>(m_textureDefaults.size());
		size_t texUploadHeapOffset = 0;
		UINT numTexsProcessed = 0;

		for (numTexsProcessed; numTexsProcessed < numTextureUploads; ++numTexsProcessed)
		{
			auto& currDefault = *m_textureDefaults[numTexsProcessed];
			if ((texUploadHeapOffset + currDefault.GetTextureDataSize()) > m_textureUploadHeap->GetResourceDesc().Width)
			{
				break;
			}

			memcpy(m_textureUploadHeap->GetMappedBuffer() + texUploadHeapOffset, currDefault.GetTexData().get(), currDefault.GetTextureDataSize());

			texUploadHeapOffset += currDefault.GetTextureDataSize();
			texUploadHeapOffset = ElysiaHelper::AlignU64(texUploadHeapOffset, 512);

			CopyTextureRegion(currDefault, *m_textureUploadHeap, texUploadHeapOffset, 
				currDefault.GetSubResourceLayouts(), currDefault.GetNumSubResources());
		}

		if (numTexsProcessed > 0)
		{
			m_textureDefaults.erase(m_textureDefaults.begin(), m_textureDefaults.begin() + numTexsProcessed);
		}
	}
}