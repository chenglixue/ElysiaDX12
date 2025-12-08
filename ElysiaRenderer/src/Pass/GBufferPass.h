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
		virtual void UpdateVariant() override;

		virtual void Dispose() override;

	private:
		struct ShaderPasseIDs
		{
			static int GBufferPassID;
		};
		struct ShaderIDs
		{
			static size_t screenSize;
			static size_t viewMatrix;
			static size_t viewMatrix_I;
			static size_t projMatrix;
			static size_t projMatrix_I;
			static size_t viewProjMatrix;
			static size_t viewProjMatrix_I;
			static size_t worldMatrix;
			static size_t opacity;
			static size_t cutoff;
			static size_t baseColorTexIndex;
			static size_t normalTexIndex;
			static size_t metallicTexIndex;
			static size_t roughnessTexIndex;
			static size_t specularTexIndex;
			static size_t baseColorTint;
			static size_t ambientCubemapTint;
			static size_t normalIntensity;
			static size_t metallicIntensity;
			static size_t roughnessIntensity;
			static size_t ambientCubemapIntensity;
			static size_t g_hasNormalTex;
			static size_t GBuffer0Index;
			static size_t GBuffer1Index;
			static size_t GBuffer2Index;
			static size_t GBuffer3Index;
			static size_t GBuffer4Index;
			static size_t GBuffer5Index;
		};
		std::vector<std::unique_ptr<RenderTexture>> m_GBufferRTs{};

		std::vector<DX12TextureResource*> GetGBuffers();
		void CreateRTs();
		void UpdateGBufferPassVariant(UINT passIndex);
		void DrawMesh(UINT passIndex);
		void DrawGBufferPass();
	};
}