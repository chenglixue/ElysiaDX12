#pragma once
#include "Helper.h"
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class AOPass : BasePass
	{
	public:
		AOPass() = default;
		AOPass(DX12Camera* pCamera);
		virtual ~AOPass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<RenderTexture> m_pAORT = nullptr;

		struct ShaderPasseIDs
		{
			static int AOPassID;
		};

		void DoCalcAO();
	};
}