#pragma once
#include "BasePass.h"
#include "lib/Utility/RenderTexture.h"
#include "lib/DX12/DX12Device.h"

namespace ElysiaRenderer
{
	class GBufferPass : public BasePass
	{
	public:
		GBufferPass(DX12Camera* pCamera);
		virtual ~GBufferPass() override;

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void UpdatePSO() override;

		virtual void Dispose() override;

	private:
		struct ShaderPasseIDs
		{
			static int GBufferPassID;
		};
		std::vector<std::unique_ptr<RenderTexture>> m_GBufferRTs{};

		std::vector<DX12TextureResource*> GetGBuffers();
		void CreateRTs();
		void BindToShader();
		void CreatePSO();
	};
}