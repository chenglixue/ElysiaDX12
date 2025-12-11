#pragma once
#include "IManager.h"

namespace ElysiaRenderer
{
	class DX12TextureResource;

	class TextureManager : public IManager
	{
	public:
		TextureManager() = default;
		TextureManager(const TextureManager& rhs) = delete;
		TextureManager& operator=(TextureManager& rhs) = delete;
		TextureManager(TextureManager&& rhs) = default;
		~TextureManager(); 

		static TextureManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []()
			{
				m_instance.reset(new TextureManager());
			});

			return *m_instance;
		}

		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;

		void LoadGlobalTextures();

		void AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource);

	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<TextureManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::vector<std::unique_ptr<DX12TextureResource>> m_textureResources{};
		std::unordered_map<std::string, UINT> m_globalRTIndexs{};
	};
}