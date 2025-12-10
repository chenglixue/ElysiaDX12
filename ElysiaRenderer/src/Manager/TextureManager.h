#pragma once
#include "IManager.h"

namespace ElysiaRenderer
{
	class DX12TextureResource;

	class TextureManager : public IManager
	{
	public:
		struct RenderTextureIDs
		{
			static size_t GGX_E_LUTID;
			static size_t GGX_Eavg_LUTID;
			static size_t SkyboxID;
			static size_t BlueNoiseID;
		};
		
		TextureManager() = default;
		TextureManager(const TextureManager& rhs) = delete;
		TextureManager& operator=(TextureManager& rhs) = delete;
		TextureManager(TextureManager&& rhs) = default;
		~TextureManager();

		static TextureManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new TextureManager());
				});

			return *m_instance;
		}

		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;

		void LoadGlobalTextures();

		void AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource, size_t nameHash);
		UINT GetTextureHeapIndex(size_t nameHash) const noexcept;

	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<TextureManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::unordered_map<size_t, std::unique_ptr<DX12TextureResource>> m_textureResources{};
	};
}