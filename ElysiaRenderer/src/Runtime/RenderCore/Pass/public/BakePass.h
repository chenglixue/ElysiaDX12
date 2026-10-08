#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{


    class BakePass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t PreIntegrateSSSLUTID = PropertyToID(L"Pre Integrate SSS LUT");
            static inline size_t IntegrateSSSNDFLUTID = PropertyToID(L"Integrate SSS NDF LUT");
            static inline size_t PreIntegrateDiffuseID = PropertyToID(L"Pre Integrate Diffuse");
            static inline size_t SobolNoiseTexID = PropertyToID(L"Sobol Noise RT");
        };

        BakePass();
        virtual ~BakePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
#define BAKE_PASS_LIST(X) \
        X(CS_CALC_SOBOL_NOISE,           "public\\CS_CalcSobolNoise.hlsl",              true,  CalcSobolNoise) \
        X(CS_PRE_INTEGRATE_SSS,          "public\\PreGen\\CS_PreIntegrateSSS.hlsl",     true,  PreIntegrateSSS) \
        X(CS_INTEGRATE_SSS_NDF,          "public\\PreGen\\CS_PreIntegrateSSS.hlsl",     true,  IntegrateSSSNDF) \
        X(CS_TEMP_SH_Coefficients,       "public\\PreGen\\CS_SHCoefficients.hlsl",      true,  CalcTempSHCoefficients) \
        X(CS_SH_Coefficients,            "public\\PreGen\\CS_SHCoefficients.hlsl",      true,  CalcSHCoefficients)
        DECLARE_SHADER_PASSES(BAKE_PASS_LIST, BAKE_PASS_COUNT);
#undef BAKE_PASS_LIST

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        constexpr static UINT m_SobolNoiseRTWidth = 128;
        constexpr static UINT m_SobolNoiseRTHeight = 128;
        bool m_bIsBakeSHCoefficients = false;
        Vector4 m_SHCoefficientsTempCount;
        EnvironmentData m_GIData{};
        SubsurfaceScatterData m_subsurfaceScatterData{};
        BufferHandle m_pSHCoefficientsTempBuffer = nullptr;
        RenderTexture* m_pSobolNoiseTex = nullptr;

        struct ShaderIDs
        {
            static inline size_t g_PreIntegrateSSSLUTIndex = PropertyToID(L"g_PreIntegrateSSSLUTIndex");
            static inline size_t g_IntegrateSSSNDFLUTIndex = PropertyToID(L"g_IntegrateSSSNDFLUTIndex");
            static inline size_t g_EnvironmentTexIndex = PropertyToID(L"g_EnvironmentTexIndex");
            static inline size_t g_SHCoefficientsBufferIndex = PropertyToID(L"g_SHCoefficientsBufferIndex");
            static inline size_t g_SHCoefficientsTempBufferIndex = PropertyToID(L"g_SHCoefficientsTempBufferIndex");
            static inline size_t g_SobolNoiseTexIndex = PropertyToID(L"g_SobolNoiseTexIndex");

            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SkyboxSize = PropertyToID(L"g_SkyboxSize");
            static inline size_t g_SHCoefficientsTempCount = PropertyToID(L"g_SHCoefficientsTempCount");
        };

        struct alignas(16) SHCoefficientData
        {
            Vector4 SHCoefficients[9]; // 这个 8x8 区域的 9 个系数的局部累加和
            float TotalWeight; // 这个 8x8 区域的立体角权重之和
        };

        void DoSobolNoise();
        void DoPreIntegrateSSSLUT();
        void DoIntegrateSSSNDFLUT();
        void DoSHCoefficients();
        void DoCalcSHCoefficients();
        void DoCalcTempSHCoefficients();
    };
}