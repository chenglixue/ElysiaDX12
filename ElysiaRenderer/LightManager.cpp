#include "stdafx.h"
#include "LightManager.h"

#include "UserData.h"

namespace ElysiaRenderer
{
	std::unique_ptr<LightManager> g_pLightManager = nullptr;

	LightManager::~LightManager()
	{
		Destory();
	}

	void LightManager::Init()
	{
		CreatMainLight();
	}

	void LightManager::Destory()
	{

	}

	void LightManager::Update()
	{
		auto& pUsetData = UserData::GetInstance();

		m_mainLight->m_lightColor = pUsetData.lightColor;
		m_mainLight->m_lightDir = pUsetData.lightDir;
		m_mainLight->m_lightIntensity = pUsetData.lightIntensity;
	}

	DX12DirectionLight* LightManager::GetMainLight()
	{
		return m_mainLight.get();
	}

	void LightManager::CreatMainLight()
	{
		auto& pUserData = UserData::GetInstance();
		if (m_mainLight != nullptr)
		{
			m_mainLight.reset();
			m_mainLight = std::make_unique<DX12DirectionLight>(pUserData.lightColor, pUserData.lightDir, pUserData.lightIntensity);
		}
		else
		{
			m_mainLight = std::make_unique<DX12DirectionLight>(pUserData.lightColor, pUserData.lightDir, pUserData.lightIntensity);
		}
	}

	LightManager* GetLightManager()
	{
		return g_pLightManager.get();
	}
}