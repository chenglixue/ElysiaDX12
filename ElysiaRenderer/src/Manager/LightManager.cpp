#include "stdafx.h"
#include "LightManager.h"

#include "Parameter/UserData.h"

namespace ElysiaRenderer
{
	std::unique_ptr<LightManager> LightManager::m_instance;
	std::once_flag LightManager::m_initInstanceFlag;
	
	LightManager::~LightManager()
	{
		Destory();
	}

	void LightManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
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
}