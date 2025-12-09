#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
	class RenderTexture;

	class OpaquePass : public BasePass
	{
	public:
		OpaquePass(DX12Camera* pCamera);
		virtual ~OpaquePass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void UpdatePSO() override;
		virtual void UpdateVariant() override;

		virtual void Dispose() override;

	private:
		struct ShaderPasseIDs
		{
			static int OpaqueLightPassID;
		};
		struct ShaderIDs
		{
			static size_t g_AOIndex;
			
			static size_t screenSize;
			static size_t viewMatrix;
			static size_t viewMatrix_I;
			static size_t projMatrix;
			static size_t projMatrix_I;
			static size_t viewProjMatrix;
			static size_t viewProjMatrix_I;
		};

		DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;

		void UpdateLightingPassVariant(UINT passID);
		void DrawLightingPass();
	};
}