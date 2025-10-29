#include "stdafx.h"
#include "TextureManager.h"

#include "DX12TextureBuffer.h"


namespace ElysiaRenderer
{
	std::unique_ptr<TextureManager> TextureManager::m_instance;
	std::once_flag TextureManager::m_initInstanceFlag;

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