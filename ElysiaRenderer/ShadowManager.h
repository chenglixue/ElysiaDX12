#pragma once
#include "IManager.h"
#include "IUpdate.h"
#include "DX12Shadow.h"
#include "DX12Light.h"
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12Device;
	
	class ShadowManager : public IManager, IUpdate
	{
	public:
		ShadowManager() = default;
		ShadowManager(DX12Device* pDevice);
		ShadowManager(const ShadowManager& rhs) = delete;
		ShadowManager& operator=(ShadowManager& rhs) = delete;
		ShadowManager(ShadowManager&& rhs) = default;
		~ShadowManager();

		virtual void Init() override;
		virtual void Destory() override;
		virtual void Update() override;

		DX12Shadow* GetMainShadow();

		void SetMainLight(DX12Light* mainLight);

		void CreateMainShadow(float resolution, float boundSphereRadius, DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT);

	private:

		DX12Device* m_pDevice = nullptr;
		std::unique_ptr<DX12Shadow> m_pMainShadow = nullptr;
		DX12Light* m_pMainLight = nullptr;
	};
	
}