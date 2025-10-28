#pragma once
#include "BasePass.h"
#include "RenderTexture.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
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
		std::unique_ptr<RenderTexture> m_pOpaqueRT = nullptr;

		void CreateRTs();
		void BindToShader();
		void CreatePSO();
	};
}