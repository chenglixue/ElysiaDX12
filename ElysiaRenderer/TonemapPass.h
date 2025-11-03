#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class TonemapPass : public BasePass
	{
	public:
		TonemapPass() = default;
		virtual ~TonemapPass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<DX12BufferResource> m_pBlitPassBuffer = nullptr;
		std::unique_ptr<RenderTexture> m_pTempRT = nullptr;
		std::unique_ptr<PipelineStateObject> m_BlitPSO = nullptr;
		std::unique_ptr<PipelineStateObject> m_TonemapPSO = nullptr;
		std::unique_ptr<DX12Shader> m_pixelShader = nullptr;
		void BindToShader();
		void CreatePSO();
	};
}