#include "DX12TextureBuffer.h"

namespace ElysiaRenderer
{
	DX12TextureBuffer::DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT bufferSize)
		: DX12GPUResource(resource, usageState)
	{
		m_bufferSize = bufferSize;
	}
}