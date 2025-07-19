#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	enum class TexFileFormat : uint8_t
	{
		None = 0,
		DDS = 1,
		TGA = 2
	};

	struct TextureBufferCreationDesc
	{
		const std::string texturePath;

		bool isSRGB;
		BufferTypeFlags bufferTypeFlags = BufferTypeFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
	};

	class DX12TextureBuffer : public DX12GPUResource
	{
	public:
		DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		~DX12TextureBuffer() override;

		DX12DescriptorHeapHandle GetDescriptorHeapHandle()
		{
			return m_SRVDescriptorHeapHandle;
		}

	private:
		DX12DescriptorHeapHandle m_SRVDescriptorHeapHandle{};
	};
}