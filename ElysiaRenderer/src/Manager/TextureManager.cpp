#include "stdafx.h"
#include "TextureManager.h"

#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/DX12/DX12Device.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<TextureManager> TextureManager::m_instance;
	std::once_flag TextureManager::m_initInstanceFlag;

	size_t TextureManager::RenderTextureIDs::GGX_E_LUTID = SIZE_MAX;
	size_t TextureManager::RenderTextureIDs::GGX_Eavg_LUTID = SIZE_MAX;
	size_t TextureManager::RenderTextureIDs::SkyboxID = SIZE_MAX;
	size_t TextureManager::RenderTextureIDs::BlueNoiseID = SIZE_MAX;

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

	void TextureManager::AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource, size_t nameHash)
	{
		if (pTextureResource == nullptr) return;

		m_textureResources.emplace(nameHash, std::move(pTextureResource));
	}

	void TextureManager::LoadGlobalTextures()
	{
		TextureCreationDesc texBufferCreateDesc{};

		{
			RenderTextureIDs::GGX_E_LUTID = PropertyToID("GGX_E_LUT");
			texBufferCreateDesc.texturePath = L"Tex\\GGX_E_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex), RenderTextureIDs::GGX_E_LUTID); 
		} 

		{
			RenderTextureIDs::GGX_Eavg_LUTID = PropertyToID("GGX_Eavg_LUT");
			texBufferCreateDesc.texturePath = L"Tex\\GGX_Eavg_LUT.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex), RenderTextureIDs::GGX_Eavg_LUTID);

		}

		{
			RenderTextureIDs::SkyboxID = PropertyToID("SkyboxTex");
			texBufferCreateDesc.texturePath = L"Tex\\cubemap0.dds";
			texBufferCreateDesc.isSRGB = false;
			auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));

			this->AddTextureResource(std::move(newTex), RenderTextureIDs::SkyboxID);
		}

		{
			// WCHAR assetsPath[512];
			// ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
			// std::wstring target = L"Tex\\Black.png";
			// auto path = ElysiaHelper::GetAssetFullPath(assetsPath, target.c_str());
			// texBufferCreateDesc.texturePath = path;
			// texBufferCreateDesc.isSRGB = false;
			//
			// auto newTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			//
			// RenderResource::GetInstance().GetCBVFrameVariable()->BlueNoiseTexIndex = newTex->GetResourceHeapIndex();
			//
			// this->AddTextureResource(std::move(newTex));
		}
	}

	UINT TextureManager::GetTextureHeapIndex(size_t nameHash) const noexcept
	{
		return m_textureResources.at(nameHash)->GetResourceHeapIndex();
	}
}