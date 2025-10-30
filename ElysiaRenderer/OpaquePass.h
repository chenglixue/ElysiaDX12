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

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:

		void BindToShader();
		void CreatePSO();
	};
}