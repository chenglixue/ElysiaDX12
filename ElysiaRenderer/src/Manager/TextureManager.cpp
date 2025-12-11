#include "stdafx.h"
#include "TextureManager.h"

#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/DX12/DX12Device.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<TextureManager> TextureManager::m_instance;
	std::once_flag TextureManager::m_initInstanceFlag;

	TextureManager::~TextureManager()
	{
		Destory();
	}

	void TextureManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
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

	void TextureManager::LoadGlobalTextures()
	{
		TextureCreationDesc texBufferCreateDesc{};

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_E_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex)); 
		} 

		{
			texBufferCreateDesc.texturePath = L"Tex\\GGX_Eavg_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex));

		}

		{
			texBufferCreateDesc.texturePath = L"Tex\\cubemap0.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex));
		}

		{
			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
			std::wstring target = L"blue_noise.dds";
			auto path = ElysiaHelper::GetAssetFullPath(assetsPath, target.c_str());
			texBufferCreateDesc.texturePath = path;
			texBufferCreateDesc.isSRGB = false;
			
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			
			this->AddTextureResource(std::move(newTex));
		}
	}
}