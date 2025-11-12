#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class TonemapPass : public BasePass
	{
	public:
		TonemapPass() = default;
		TonemapPass(DX12Camera* pCamera);
		virtual ~TonemapPass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<RenderTexture> m_pTempRT = nullptr;

		struct ShaderPasseIDs
		{
			static int BlitPassID;
			static int TonemapPassID;
		};
	};
}