#pragma once
#include "BasePass.h"
#include "lib/DX12/DX12Shadow.h"
#include "Manager/LightManager.h"
#include "lib/Utility/RenderTexture.h"


namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class ShadowPass : public BasePass
	{
	public:
		ShadowPass(DX12Camera* pCamera);
		virtual ~ShadowPass() override;

		//virtual void Setup(const RenderPassData& renderPasssData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void UpdatePSO() override;
		virtual ShaderVariantData UpdateVariant() override;

		virtual void Dispose() override;

		RenderTexture* GetShadowRT() const;
		void UpdateShadowPassVariant();
	private:
		std::unique_ptr<RenderTexture> m_pShadowRT = nullptr;
		std::unique_ptr<DX12Shadow> m_pMainShadow = nullptr;
		DX12DirectionLight* m_pMainLight = nullptr;

		struct ShaderPasseIDs
		{
			static int ShadowCastPassID;
		};
		struct ShaderIDs
		{
			static size_t shadowNearZ;
			static size_t shadowFarZ;
			static size_t shadowDepthBias;
			static size_t shadowSlopeDepthBias;
			static size_t shadowMaxSlopeDepthBias;
			static size_t g_sobolSequence;
			static size_t worldMatrix;
			static size_t baseColorTexIndex;
			static size_t opacity;
			static size_t cutoff;
		};

		void CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format);
	};
}