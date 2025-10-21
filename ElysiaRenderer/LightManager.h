#pragma once

#include "IManager.h"
#include "IUpdate.h"
#include "DX12Light.h"

namespace ElysiaRenderer
{
	class LightManager : public IManager, IUpdate
	{
	public:
		LightManager() = default;
		LightManager(const LightManager& rhs) = delete;
		LightManager& operator=(LightManager& rhs) = delete;
		LightManager(LightManager&& rhs) = default;
		~LightManager();

		static LightManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new LightManager());
				});

			return *m_instance;
		}

		virtual void Init() override;
		virtual void Destory() override;
		virtual void Update() override;

		DX12Light* GetMainLight();

	private:
		void CreatMainLight();

		static std::unique_ptr<LightManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::unique_ptr<DX12DirectionLight> m_mainLight = nullptr;

	};

	inline static DX12DirectionLight* GetMainLight()
	{
		return dynamic_cast<DX12DirectionLight*>(LightManager::GetInstance().GetMainLight());
	}
}