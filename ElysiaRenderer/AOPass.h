#pragma once
#include "Helper.h"
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class AOPass : public BasePass
	{
	public:
		AOPass() = default;
		AOPass(DX12Camera* pCamera);
		virtual ~AOPass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void Dispose() override;
		virtual void UpdatePSO() override;

	private:
		std::unique_ptr<RenderTexture> m_pAORT = nullptr;

		struct ShaderPasseIDs
		{
			static int AOPassID;
			static int BlitPassID;
		};

		void DoCalcAO();
		void DoBlitToBackBuffer();
		std::vector<Vector4> GenerateSSAOSampleKernel();
		DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
	};
}