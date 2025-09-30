#pragma once
#include "stdafx.h"
#include "IManager.h"
#include "DX12TextureBuffer.h"

namespace ElysiaRenderer
{
	class TextureManager : public IManager
	{
	public:
		TextureManager() = default;
		TextureManager(const TextureManager& rhs) = delete;
		TextureManager& operator=(TextureManager& rhs) = delete;
		TextureManager(TextureManager&& rhs) = default;
		~TextureManager();

		virtual void Init() override;
		virtual void Destory() override;

		void AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource);

		const std::vector<DX12TextureResource*> GetTextureResources() const noexcept;

	private:
		std::vector<std::unique_ptr<DX12TextureResource>> m_textureResources{};
	};
}