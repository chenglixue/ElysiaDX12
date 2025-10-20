#pragma once
#include "stdafx.h"
#include "DX12TextureBuffer.h"
#include "Renderer.h"

namespace ElysiaHelper
{
	using namespace ElysiaRenderer;

	struct RenderTextureDesc
	{
		UINT64 Width = 0;
		UINT64 Height = 0;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		bool EnableRandomWrite = false;
		TextureDimension Dimension;
		UINT64 MSAASamples = 1;
		UINT64 ArraySize = 1;
		UINT16 MipmapLevels = 1;
		const wchar_t* Name = nullptr;
	};

	class RenderTexture
	{
	public:
		RenderTexture() = default;
		~RenderTexture();

		void Init(RenderTextureDesc desc);
		void ShutDowm();

		UINT64 GetSRVIndex() const;
		UINT64 GetWidth() const;
		UINT64 GetHeight() const;
		DXGI_FORMAT GetFormat() const;
		ID3D12Resource* GetResource() const;
		UINT64 GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice) const;

	private:
		std::unique_ptr<DX12TextureResource> m_pTexture;
		UINT m_MSAASamples = 0;
		UINT m_MSAAQuality = 0;
	};
}