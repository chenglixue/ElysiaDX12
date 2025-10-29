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
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new TextureManager());
				});

			return *m_instance;
		}

		virtual void Init() override;
		virtual void Destory() override;

		void AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource);

		const std::vector<DX12TextureResource*> GetTextureResources() const noexcept;

	private:
		static std::unique_ptr<TextureManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::vector<std::unique_ptr<DX12TextureResource>> m_textureResources{};
	};
}