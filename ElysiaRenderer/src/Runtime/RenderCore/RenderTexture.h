#pragma once
#include "RenderTextureUtility.h"
#include "Runtime/RenderCore/TextureManager.h"

namespace ElysiaCore
{
	class DX12Device;
}

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	
	class RenderTexture
	{
	public:
		RenderTexture() = default;
		~RenderTexture();

		void Init(ElysiaCore::DX12Device* pDevice, RenderTextureDesc desc);
		void ShutDowm();

		DX12TextureResource* GetTexture() const;
		UINT64 GetSRVIndex() const;
		UINT64 GetWidth() const;
		UINT64 GetHeight() const;
		DXGI_FORMAT GetFormat() const;
		ID3D12Resource* GetResource() const;
		UINT64 GetSubresourceIndex(UINT64 mipmapLevel, UINT64 arraySlice) const;
		UINT GetResourceHeapIndex() const noexcept;

	private:
		TextureManager::Handle m_handle;
		bool m_isDepth = false;
		DXGI_FORMAT m_depthFormat;
		UINT m_MSAASamples = 0;
		UINT m_MSAAQuality = 0;
	};

	bool IsRenderTextureReady(const std::vector<ElysiaRenderer::RenderTexture*> texs);
}
