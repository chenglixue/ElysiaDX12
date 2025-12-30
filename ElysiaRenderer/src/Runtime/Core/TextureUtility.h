#pragma once
#include "stdafx.h"
#include "BufferUtility.h"
#include "Programs/Helper.h"

namespace ElysiaCore
{
	class DX12TextureResource;
}

namespace ElysiaCore
{
	const std::wstring DefaultBlackTexturePath = L"..\\Tex\\Black.png";
	const std::wstring DefaultWhiteTexturePath = L"..\\Tex\\White.png";
	const std::wstring DefaultNormalTexturePath = L"..\\Tex\\DefaultNormalMap.png";

	enum class TexTypeFlags : uint8_t
	{
		None = 0,
		RTV = 1 << 0,
		DSV = 1 << 1,
		SRV = 1 << 2,
		UAV = 1 << 3
	};
	inline TexTypeFlags operator|(TexTypeFlags l, TexTypeFlags r)
	{
		return static_cast<TexTypeFlags>(static_cast<uint8_t>(l) | static_cast<uint8_t>(r));
	}
	inline TexTypeFlags operator&(TexTypeFlags a, TexTypeFlags b)
	{
		return static_cast<TexTypeFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}

	enum TextureDimension : INT8
	{
		/// <summary>
		///   <para>Texture type is not initialized or unknown.</para>
		/// </summary>
		Unknown = -1, // 0xFFFFFFFF
		/// <summary>
		///   <para>No texture is assigned.</para>
		/// </summary>
		None = 0,
		/// <summary>
		///   <para>Any texture type.</para>
		/// </summary>
		Any = 1,
		/// <summary>
		///   <para>2D texture (Texture2D).</para>
		/// </summary>
		Tex2D = 2,
		/// <summary>
		///   <para>3D volume texture (Texture3D).</para>
		/// </summary>
		Tex3D = 3,
		/// <summary>
		///   <para>Cubemap texture.</para>
		/// </summary>
		Cube = 4,
		/// <summary>
		///   <para>2D array texture (Texture2DArray).</para>
		/// </summary>
		Tex2DArray = 5,
	};

	struct TexCreateDesc
	{
		TexCreateDesc()
		{
			m_resouceDesc.Format = DXGI_FORMAT_UNKNOWN;
			m_resouceDesc.Width = 0;
			m_resouceDesc.Height = 0;
			m_resouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			m_resouceDesc.DepthOrArraySize = 1;
			m_resouceDesc.MipLevels = 1;
			m_resouceDesc.SampleDesc.Count = 1;
			m_resouceDesc.SampleDesc.Quality = 0;
			m_resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			m_resouceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			m_resouceDesc.Alignment = 0;
		}

		std::wstring m_name{};
		D3D12_RESOURCE_DESC m_resouceDesc{};
		TexTypeFlags m_typeFlag = TexTypeFlags::None;
	};

	enum class TexFileFormat : uint8_t
	{
		None = 0,
		DDS = 1,
		TGA = 2
	};

	struct TextureCreationDesc
	{
		std::wstring texturePath;

		bool isSRGB;
	};

	struct TextureBufferCreationDesc
	{
		GPUResourceFlags bufferTypeFlags = GPUResourceFlags::None;
		BufferAccessFlags bufferAccessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
		UINT m_size = 0;
		UINT m_stride = 0;
	};

	struct DX12TextureUpload
	{
		
		DX12TextureResource* pTextureBuffer;
		std::unique_ptr<UINT8[]> pTextureData = nullptr;
		size_t textureDataSize = 0;
		UINT numSubResources = 0;
		ElysiaHelper::SubResourceLayouts subResourceLayouts{ 0 };
	};
}
