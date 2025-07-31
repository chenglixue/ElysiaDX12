#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	enum class TexFileFormat : uint8_t
	{
		None = 0,
		DDS = 1,
		TGA = 2
	};

	struct TextureCreationDesc
	{
		std::string texturePath = "";

		bool isSRGB;
	};

	struct TextureBufferCreationDesc
	{
		BufferTypeFlags bufferTypeFlags		= BufferTypeFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess					= false;
		UINT m_size							= 0;
		UINT m_stride						= 0;
	};

	extern class DX12TextureResource;
	/// <summary>
	/// only save texture data, not a heap but save a default heap
	/// </summary>
	class DX12TextureBuffer : public DX12BufferResource
	{
		
	public:
		DX12TextureBuffer(DX12TextureResource* texResource, size_t mipLevels, size_t arraySize);
		//DX12TextureBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation);

		~DX12TextureBuffer() override;

		DX12DescriptorHeapHandle GetDescriptorHeapHandle() const
		{
			return m_SRVDescriptorHeapHandle;
		}
		DX12TextureResource* GetDefaultHeap() const
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
		DX12TextureResource* m_tex = nullptr;	// default heap
		UINT m_numSubResources = 0;
		SubResourceLayouts m_subResourceLayouts;
		std::unique_ptr<uint8_t[]> m_textureData;
		size_t m_textureDataSize = 0;
		std::unique_ptr<DX12GPUResource> m_texUploadBuffer = nullptr;
	};

	class DX12TextureUploadBuffer : public DX12BufferResource
	{
	public:
		DX12TextureUploadBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12MA::Allocation* allocation);
		~DX12TextureUploadBuffer() override;

		uint8_t* GetMappedBuffer()
		{
			return m_mappedBuffer;
		}
		void Unmap()
		{
			if (m_mappedBuffer != nullptr)
			{
				m_resource->Unmap(0, nullptr);

			}
		}

	private:
		uint8_t* m_mappedBuffer;
		BufferType m_bufferType = BufferType::Texture;
	};
}