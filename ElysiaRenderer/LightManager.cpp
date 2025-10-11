#include "LightManager.h"

namespace ElysiaRenderer
{
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
		/*m_mainLight->m_lightColor = g_userData.lightColor;
		m_mainLight->m_lightDir = g_userData.lightDir;
		m_mainLight->m_lightIntensity = g_userData.lightIntensity;*/
	}

	DX12Light* LightManager::GetMainLight()
	{
		return m_mainLight.get();
	}

	void LightManager::CreatMainLight()
	{
		if (m_mainLight != nullptr)
		{
			m_mainLight.reset();
			m_mainLight = std::make_unique<DX12DirectionLight>(g_userData.lightColor, g_userData.lightDir, g_userData.lightIntensity);
		}
		else
		{
			m_mainLight = std::make_unique<DX12DirectionLight>(g_userData.lightColor, g_userData.lightDir, g_userData.lightIntensity);
		}
	}
}