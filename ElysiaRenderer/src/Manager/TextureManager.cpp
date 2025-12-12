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

		m_pBindlessTextureManager = std::make_unique<BindlessTextureManager>();
		m_pBindlessTextureManager->Init(pDevice);
	}
	void TextureManager::Destory()
	{

	}

	TextureManager::Handle TextureManager::LoadDynamicTexture(const std::wstring& filePath, bool isSRGB)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		auto nameHash = xxh::GetHash(filePath);
		auto it = m_dynamicTextureMap.find(nameHash);
		if (it != m_dynamicTextureMap.end())
		{
			it->second->refCount++;
			return {it->second->textureHandle, filePath};
		}

		auto handle = m_pBindlessTextureManager->CreateTextureFromFile(filePath, isSRGB);
		if (!handle.IsValid()) return Handle::Invalid();

		auto managed = std::make_shared<ManagedTexture>();
		managed->textureHandle = handle;
		managed->flag = LoadFlags::Dynamic;
		managed->refCount = 1;

		m_dynamicTextureMap.emplace(nameHash, managed);
#ifdef DEBUG
		m_debugTextureMap.emplace(filePath, managed);
#endif

		return {handle, filePath};
	}

	TextureManager::Handle TextureManager::LoadResidentTexture(const std::wstring& filePath, bool isSRGB)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		auto nameHash = xxh::GetHash(filePath);
		auto it = m_dynamicTextureMap.find(nameHash);
		if (it != m_dynamicTextureMap.end())
		{
			return {it->second->textureHandle, filePath};
		}

		auto handle = m_pBindlessTextureManager->CreateTextureFromFile(filePath, isSRGB);
		if (!handle.IsValid()) return Handle::Invalid();

		auto managed = std::make_shared<ManagedTexture>();
		managed->textureHandle = handle;
		managed->flag = LoadFlags::Resident;

		m_dynamicTextureMap.emplace(nameHash, managed);
#ifdef DEBUG
		m_debugTextureMap.emplace(filePath, managed);
#endif

		return {handle, filePath};
	}

	TextureManager::Handle TextureManager::CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc, TexTypeFlags flag, std::wstring name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		auto nameHash = xxh::GetHash(name);
		auto it = m_dynamicTextureMap.find(nameHash);
		if (it != m_dynamicTextureMap.end())
		{
			it->second->refCount++;
			return {it->second->textureHandle, name};
		}

		auto handle = m_pBindlessTextureManager->CreateTexture(resourceDesc, flag, name);
		if (!handle.IsValid()) return Handle::Invalid();

		auto managed = std::make_shared<ManagedTexture>();
		managed->textureHandle = handle;
		managed->refCount = 1;

		m_dynamicTextureMap.emplace(nameHash, managed);
#ifdef DEBUG
		m_debugTextureMap.emplace(name, managed);
#endif

		return {handle, name};
	}

	int TextureManager::GetReferenceCount(const std::wstring& filePath) 
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		auto nameHash = xxh::GetHash(filePath);

		return m_dynamicTextureMap.contains(nameHash) ? m_dynamicTextureMap.at(nameHash)->refCount.load() : 0;
	}

	size_t TextureManager::GetTotalManagedCount() noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		return m_dynamicTextureMap.size();
	}

	DX12TextureResource* TextureManager::GetTexture(Handle handle) const
	{
		return m_pBindlessTextureManager->GetTexture(handle.textureHandle);
	}
	UINT TextureManager::GetResourceHeapIndex(Handle handle) const
	{
		return GetTexture(handle)->GetResourceHeapIndex();
	}

	void TextureManager::ShutDown()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_dynamicTextureMap.clear();
		m_residentTextureMap.clear();
		m_pBindlessTextureManager->Destory();
	}

	void TextureManager::Release(Handle handle)
	{
		if (!handle.IsValid()) return;

		std::lock_guard<std::mutex> lock(m_mutex);

		auto pathHash = xxh::GetHash(handle.filePath);
		auto it = m_dynamicTextureMap.find(pathHash);
		if (it != m_dynamicTextureMap.end())
		{
			auto& managed = it->second;
			if (managed->IsResident())
			{
				return;
			}
			int newCount = managed->refCount--;

			if (newCount <= 0)
			{
				m_dynamicTextureMap.erase(it);
			}
		}

		handle.Invalid();
	}
	
	void TextureManager::PrintStats()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::wcout << L"[TextureManager] Total Managed Textures: " << m_dynamicTextureMap.size() << std::endl;

#ifdef DEBUG
		for (const auto& kv : m_debugTextureMap)
		{
			std::wcout << L"  " << kv.first << L" (Refs: " << kv.second->refCount.load() << L")\n";
		}
#endif
	}
}