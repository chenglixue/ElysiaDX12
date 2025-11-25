#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class FinalBlitPass : public BasePass
	{
	public:
		FinalBlitPass() = default;
		FinalBlitPass(DX12Camera* pCamera);
		virtual ~FinalBlitPass() override;

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void UpdatePSO() override;

		virtual void Dispose() override;

	private:
		struct ShaderPassIDs
		{
			static int BlitPassID;
		};
		
		DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_UNKNOWN;

	};
}