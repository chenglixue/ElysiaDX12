#pragma once
#include "stdafx.h"
#include "GPUResource.h"

namespace ElysiaRenderer
{
	class DX12VertexBuffer : public DX12GPUResource
	{
	public:
		DX12VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, uint32_t vertexStride, uint32_t bufferSize);
		~DX12VertexBuffer() override;

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView()
		{
			return m_vertexBufferView;
		}

	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	};
}