// #pragma once
// #include "lib/Utility/Helper.h"
// #include "BasePass.h"
//
// namespace ElysiaRenderer
// {
// 	class RenderTexture;
//
// 	class AOPass : public BasePass
// 	{
// 	public:
// 		AOPass() = default;
// 		AOPass(DX12Camera* pCamera);
// 		virtual ~AOPass() override;
//
// 		virtual void Configure() override;
// 		virtual void Execute() override;
// 		virtual void Render() override;
// 		virtual void Dispose() override;
// 		virtual void UpdatePSO() override;
// 		virtual void UpdateVariant() override;
// 		
// 		void UpdateGBufferPassVariant(UINT passIndex);
// 	private:
// 		RenderTexture* m_pAORT = nullptr;
//
// 		struct ShaderPasseIDs
// 		{
// 			static int AOPassID;
// 			static int BlitPassID;
// 		};
// 		struct RenderTextureIDs
// 		{
// 			static size_t AORTID;
// 			static size_t AOTempRTID;
// 		};
// 		struct ShaderIDs
// 		{
// 			static size_t g_ScreenSize;
// 			static size_t viewMatrix;
// 			static size_t viewMatrix_I;
// 			static size_t projMatrix;
// 			static size_t projMatrix_I;
// 			static size_t viewProjMatrix;
// 			static size_t viewProjMatrix_I;
// 			
// 			static size_t g_AOSampleKernelArray;
// 			static size_t g_AOSampleCount;
// 			static size_t g_AORadius;
// 			static size_t g_AOIntensityMul;
// 			static size_t g_AOIntensityPow;
//
// 			static size_t g_AOIndex;
// 			static size_t blitterTextureIndex;
// 		};
//
// 		void DoCalcAO();
// 		void DoBlitToBackBuffer();
// 		std::vector<Vector4> GenerateSSAOSampleKernel();
// 		DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
// 	};
// }