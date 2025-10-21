#pragma once
#include "BasePass.h"
#include "DX12Shadow.h"
#include "LightManager.h"
#include "RenderTexture.h"


namespace ElysiaRenderer
{
	class ShadowPass : public BasePass
	{
	public:
		ShadowPass() = default;
		~ShadowPass() override;

		static ShadowPass& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new ShadowPass());
				});

			return *m_instance;
		}

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

		RenderTexture* GetShadowRT() const;

	private:
		static std::unique_ptr<ShadowPass> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::unique_ptr<RenderTexture> m_pShadowRT = nullptr;
		std::unique_ptr<DX12Shadow> m_pMainShadow = nullptr;
		DX12DirectionLight* m_pMainLight = nullptr;

		void CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format);
	};
}