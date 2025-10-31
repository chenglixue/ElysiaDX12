#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class OpaquePass : public BasePass
	{
	public:
		OpaquePass() = default;
		virtual ~OpaquePass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::unique_ptr<PipelineStateObject> m_PSO = nullptr;
		std::unique_ptr<DX12Shader> m_pixelShader = nullptr;
		void BindToShader();
		void CreatePSO();
	};
}