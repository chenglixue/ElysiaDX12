#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	struct DepthBufferCreateDesc
	{
		DepthBufferCreateDesc()
		{
			m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			m_resourceDesc.Width = 0;
			m_resourceDesc.Height = 0;
			m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			m_resourceDesc.DepthOrArraySize = 1;
			m_resourceDesc.MipLevels = 1;
			m_resourceDesc.SampleDesc.Count = 1;
			m_resourceDesc.SampleDesc.Quality = 0;
			m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			m_resourceDesc.Alignment = 0;
		}

		D3D12_RESOURCE_DESC m_resourceDesc{};
		BufferTypeFlags bufferTypeFlags = BufferTypeFlags::DSV;
	};

	class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState)
			: DX12GPUResource(resource, usageState)
		{

		}

		~DX12BufferResource()
		{
			Destory();
		}
	};
}
