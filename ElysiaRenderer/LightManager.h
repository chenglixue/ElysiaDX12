#pragma once

#include "IManager.h"
#include "IUpdate.h"
#include "DX12Light.h"
#include "UserData.h"

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

		virtual void Init() override;
		virtual void Destory() override;
		virtual void Update() override;

		DX12Light* GetMainLight();

	private:
		void CreatMainLight();
		std::unique_ptr<DX12DirectionLight> m_mainLight = nullptr;

	};
}