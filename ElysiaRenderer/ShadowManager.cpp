#include "ShadowManager.h"
#include "DX12Device.h"
#include "UserData.h"

namespace ElysiaRenderer
{
	ShadowManager::ShadowManager(DX12Device* pDevice)
		: m_pDevice(std::move(pDevice))
	{

	}

	ShadowManager::~ShadowManager()
	{

	}

	void ShadowManager::Init()
	{
	}

	void ShadowManager::Destory() 
	{

	}

	void ShadowManager::Update()
	{
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);
	}

	DX12Shadow* ShadowManager::GetMainShadow()
	{
		return m_pMainShadow.get();
	}

	void ShadowManager::SetMainLight(DX12Light* mainLight)
	{
		assert(mainLight != nullptr);

		m_pMainLight = mainLight;
	}

	void ShadowManager::CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format)
	{
		TexCreateDesc shadowCreateDesc{};
		shadowCreateDesc.m_name = L"Shadowm RT";
		float resolution = 512;
		switch (UserData::GetInstance().shadowQuality)
		{
			case ShadowQuality::Low:
			{
				resolution = 512;
				break;
			}
			case ShadowQuality::Middle:
			{
				resolution = 1024;
				break;
			}
			case ShadowQuality::High:
			{
				resolution = 2048;
				break;
			}
			case ShadowQuality::VeryHigh:
			{
				resolution = 4096;
				break;
			}
			default:
			{
				resolution = 1024;
				ThrowRuntimeError("inivalid shadow quality");
				break;
			}
		}
		shadowCreateDesc.m_resouceDesc.Width = resolution;
		shadowCreateDesc.m_resouceDesc.Height = resolution;
		shadowCreateDesc.m_resouceDesc.Format = format;
		shadowCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

		std::unique_ptr<DX12TextureResource> shadowTex = std::move(m_pDevice->CreateTexture(shadowCreateDesc));
		auto shadowMap = std::make_unique<DX12Shadow>(std::move(shadowTex));
		shadowMap->InitBoundSphere(1000);

		if (m_pMainShadow != nullptr)
		{
			m_pMainShadow.reset();
			m_pMainShadow = std::move(shadowMap);
		}
		else
		{
			m_pMainShadow = std::move(shadowMap);
		}
	}
}