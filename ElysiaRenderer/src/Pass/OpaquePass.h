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

		virtual void Dispose() override;

	private:
		struct ShaderPasseIDs
		{
			static int OpaqueLightPassID;
		};

		DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
	};
}