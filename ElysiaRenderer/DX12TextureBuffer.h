#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"

namespace ElysiaRenderer
{
	class DX12TextureBuffer : public DX12GPUResource
	{
	public:
		DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT bufferSize);
		//~DX12TextureBuffer();

	private:
		UINT m_bufferSize;
	};
}