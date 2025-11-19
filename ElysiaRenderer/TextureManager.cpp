#include "stdafx.h"
#include "TextureManager.h"

#include "DX12TextureBuffer.h"
#include "DX12Device.h"
#include "RenderResource.h"

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
		LoadGlobalTextures();
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

	void TextureManager::LoadGlobalTextures()
	{
		TextureCreationDesc texBufferCreateDesc{};

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_E_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			GetRenderResource()->GetCBVFrameVariable()->GGX_E_LUT_Index = newTex->GetResourceHeapIndex();
			 
			this->AddTextureResource(std::move(newTex)); 
		} 

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_Eavg_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			GetRenderResource()->GetCBVFrameVariable()->GGX_Eavg_LUT_Index = newTex->GetResourceHeapIndex();

			this->AddTextureResource(std::move(newTex));

		}

		{
			texBufferCreateDesc.texturePath = L"Tex\\cubemap0.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			GetRenderResource()->GetCBVFrameVariable()->SkyboxTexIndex = newTex->GetResourceHeapIndex();

			this->AddTextureResource(std::move(newTex));
		}

		{
			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
			texBufferCreateDesc.texturePath = StringToWstring(std::filesystem::path(assetsPath).string() + "Tex\\bluenoise_frd_1024x1024.png");
			texBufferCreateDesc.isSRGB = false;

			auto newTex = std::move(GetDevice()->CreateTextureFromFile(texBufferCreateDesc));

			GetRenderResource()->GetCBVFrameVariable()->BlueNoiseTexIndex = newTex->GetResourceHeapIndex();

			this->AddTextureResource(std::move(newTex));
		}
	}
}