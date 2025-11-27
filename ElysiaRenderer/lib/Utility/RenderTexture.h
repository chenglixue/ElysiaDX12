#pragma once
#include "../DX12/DX12TextureBuffer.h"
#include "RenderTextureUtility.h"

namespace ElysiaRenderer
{
	class RenderTexture
	{
	public:
		RenderTexture() = default;
		~RenderTexture();

		void Init(RenderTextureDesc desc);
		void ShutDowm();

		DX12TextureResource* GetTexture() const;
		UINT64 GetSRVIndex() const;
		UINT64 GetWidth() const;
		UINT64 GetHeight() const;
		DXGI_FORMAT GetFormat() const;
		ID3D12Resource* GetResource() const;
		UINT64 GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice) const;

	private:
		std::unique_ptr<DX12TextureResource> m_pTexture;
		bool m_isDepth = false;
		DXGI_FORMAT m_depthFormat;
		UINT m_MSAASamples = 0;
		UINT m_MSAAQuality = 0;
	};

	inline std::unique_ptr<RenderTexture> CreateRenderTexture(
		UINT64 width, 
		UINT64 height,
		DXGI_FORMAT format,
		const wchar_t* name = L"")
	{
		RenderTextureDesc desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = format;
		desc.Name = name;

		auto cameraDepthRT = std::make_unique<RenderTexture>();
		cameraDepthRT->Init(desc);

		return cameraDepthRT;
	}

	inline std::unique_ptr<RenderTexture> CreateRenderTexture(
		UINT64 width,
		UINT64 height,
		DXGI_FORMAT format,
		bool isDepth,
		const wchar_t* name = L"")
	{
		RenderTextureDesc desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = format;
		desc.Name = name;
		desc.IsDepth = isDepth;

		auto cameraDepthRT = std::make_unique<RenderTexture>();
		cameraDepthRT->Init(desc);

		return cameraDepthRT;
	}

	inline std::unique_ptr<RenderTexture> CreateRWRenderTexture(
		UINT64 width,
		UINT64 height,
		DXGI_FORMAT format,
		bool enableRandomWrite,
		const wchar_t* name = L"")
	{
		RenderTextureDesc desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = format;
		desc.Name = name;
		desc.EnableRandomWrite = enableRandomWrite;

		auto cameraDepthRT = std::make_unique<RenderTexture>();
		cameraDepthRT->Init(desc);

		return cameraDepthRT;
	}
}