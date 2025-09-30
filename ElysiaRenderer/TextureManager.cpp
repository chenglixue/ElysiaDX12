#include "TextureManager.h"

namespace ElysiaRenderer
{
	TextureManager::~TextureManager()
	{
		Destory();
	}

	void TextureManager::Init()
	{

	}
	void TextureManager::Destory()
	{

	}

	void TextureManager::AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource)
	{
		if (pTextureResource == nullptr) return;

		m_textureResources.emplace_back(std::move(pTextureResource));
	}

	const std::vector<DX12TextureResource*> TextureManager::GetTextureResources() const noexcept
	{
		std::vector<DX12TextureResource*> o{ m_textureResources.size()};

		for (size_t i = 0; i < m_textureResources.size(); ++i)
		{
			o[i] = m_textureResources[i].get();
		}

		return o;
	}
}