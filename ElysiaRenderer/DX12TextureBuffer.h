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
		std::string texturePath = "";

		bool isSRGB;
		BufferTypeFlags bufferTypeFlags = BufferTypeFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
	};

	extern class DX12TextureResource;
	class DX12TextureBuffer : public DX12GPUResource
	{
		using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;
		
	public:
		DX12TextureBuffer(DX12TextureResource* texResource, size_t mipLevels, size_t arraySize);
		~DX12TextureBuffer() override;

		DX12DescriptorHeapHandle GetDescriptorHeapHandle() const
		{
			return m_SRVDescriptorHeapHandle;
		}
		DX12TextureResource* GetTexResource() const
		{
			return m_tex;
		}
		UINT GetNumSubResources() const
		{
			return m_numSubResources;
		}
		std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>& GetSubResourceLayouts()
		{
			return m_subResourceLayouts;
		}
		size_t& GetTextureDataSize() 
		{
			return m_textureDataSize;
		}
		std::unique_ptr<uint8_t[]>& GetTexData()
		{
			return m_textureData;
		}

		void InitTexData()
		{
			m_textureData = std::make_unique<uint8_t[]>(m_textureDataSize);
		}
	private:

		DX12DescriptorHeapHandle m_SRVDescriptorHeapHandle{};
		DX12TextureResource* m_tex = nullptr;
		UINT m_numSubResources = 0;
		std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT> m_subResourceLayouts;
		std::unique_ptr<uint8_t[]> m_textureData;
		size_t m_textureDataSize = 0;
		std::unique_ptr<DX12GPUResource> m_texUploadBuffer = nullptr;
	};
}