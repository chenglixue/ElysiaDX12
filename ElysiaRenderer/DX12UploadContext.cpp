#include"DX12UploadContext.h"

namespace ElysiaRenderer
{
	DX12UploadContext::DX12UploadContext(DX12Device* device,
		//std::unique_ptr<DX12VertexBuffer> vertexUploadHeap,
		std::unique_ptr<DX12TextureBuffer> textureUploadHeap)
		: DX12Context(device, D3D12_COMMAND_LIST_TYPE_COPY),
		//m_vertexUploadHeap(std::move(vertexUploadHeap)),
		m_textureUploadHeap(std::move(textureUploadHeap))
	{

	}

	DX12UploadContext::~DX12UploadContext()
	{

	}
}