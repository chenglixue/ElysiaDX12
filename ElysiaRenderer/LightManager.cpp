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

	DX12Light* LightManager::GetMainLight()
	{
		return m_mainLight.get();
	}

	void LightManager::CreatMainLight()
	{
		if (m_mainLight != nullptr)
		{
			m_mainLight.reset();
			m_mainLight = std::make_unique<DX12DirectionLight>(Vector3(1, 1, 1), Vector3(0.f, -1.f, 0.f), 1);
		}
		else
		{
			m_mainLight = std::make_unique<DX12DirectionLight>(Vector3(1, 1, 1), Vector3(0.f, -1.f, 0.f), 1);
		}
	}
}