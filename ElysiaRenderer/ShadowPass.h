#pragma once
#include "BasePass.h"
#include "DX12Shadow.h"
#include "LightManager.h"

namespace ElysiaRenderer
{
	class ShadowPass : public BasePass
	{
	public:
		ShadowPass() = default;
		~ShadowPass() override;

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<DX12Shadow> m_pMainShadow = nullptr;
	};
}