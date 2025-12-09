#pragma once
#include "../DX12/DX12TextureBuffer.h"
#include "RenderTextureUtility.h"

namespace ElysiaRenderer
{
	class DX12Device;
}

namespace ElysiaRenderer
{
	class RenderTexture
	{
	public:
		RenderTexture() = default;
		~RenderTexture();

		void Init(DX12Device* pDevice, RenderTextureDesc desc);
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
		std::unique_ptr<DX12TextureResource> m_pTexture;
		bool m_isDepth = false;
		DXGI_FORMAT m_depthFormat;
		UINT m_MSAASamples = 0;
		UINT m_MSAAQuality = 0;
	};

	inline bool IsRenderTextureReady(const std::vector<ElysiaRenderer::RenderTexture*> texs)
	{
		bool isReady = true;

		for (const auto& tex : texs)
		{
			if (tex->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null tex resource");;
			}
			isReady &= tex->GetTexture()->GetIsReady();
		}

		return isReady;
	}
}
