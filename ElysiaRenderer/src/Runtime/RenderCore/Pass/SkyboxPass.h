#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class SkyboxPass : public BasePass
	{
	public:
		SkyboxPass() = default;
		virtual ~SkyboxPass() override;

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Render(ElysiaEngine::FrameContext frameContext) override;
		virtual void UpdatePSO() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<RenderTexture> m_pOpaqueRT = nullptr;

		void CreatePSO();
	};
}
