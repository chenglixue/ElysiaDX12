#pragma once
#include "Utility/Helper.h"
#include "BasePass.h"

namespace ElysiaRenderer
{
	class BloomPass : public BasePass
	{
	public:
		BloomPass() = default;
		BloomPass(DX12Camera* pCamera);
		virtual ~BloomPass() override;

		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;
		virtual void Dispose() override;
		virtual void UpdatePSO() override;

	private:
		std::unique_ptr<RenderTexture> m_pBloomRT = nullptr;

		struct ShaderPasseIDs
		{
			static int BloomPassID;
			static int BlitPassID;
		};

		struct ShaderIDs
		{
			static size_t g_DestTextureIndexID;
		};

		void DoBloomPass();
	};
}
