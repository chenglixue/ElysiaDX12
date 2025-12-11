#include "stdafx.h"
#include "LightManager.h"

#include "Parameter/UserData.h"
#include "src/Pass/ShadowPass.h"

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

		m_pMainLight->m_lightColor = pUsetData.lightColor;
		m_pMainLight->m_lightDir = pUsetData.lightDir;
		m_pMainLight->m_lightIntensity = pUsetData.lightIntensity;

		m_pMainLight->GetMainShadow()->UpdateShadowTransform(m_pMainLight.get());
	}

	DX12DirectionLight* LightManager::GetMainLight()
	{
		return m_pMainLight.get();
	}
	DX12Shadow* LightManager::GetMainShadow()
	{
		return m_pMainLight->GetMainShadow();
	}
	RenderTexture* LightManager::GetMainShadowRT() const
	{
		return m_pMainLight->GetMainShadowRT();
	}

	void LightManager::CreatMainLight()
	{
		auto& pUserData = UserData::GetInstance();
		if (m_pMainLight != nullptr)
		{
			m_pMainLight.reset();
			m_pMainLight = std::make_unique<DX12DirectionLight>(pUserData.lightColor, pUserData.lightDir, pUserData.lightIntensity);
		}
		else
		{
			m_pMainLight = std::make_unique<DX12DirectionLight>(pUserData.lightColor, pUserData.lightDir, pUserData.lightIntensity);
		}
		
		m_pMainLight->CreateMainShadow(1000, DXGI_FORMAT_D24_UNORM_S8_UINT);
	}
}