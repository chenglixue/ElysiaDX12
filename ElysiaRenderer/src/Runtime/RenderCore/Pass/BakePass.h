#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderPassResourceManager.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{


    class BakePass : public BasePass
    {
    public:
#define BAKE_PASS_LIST \
        PASS(CS_PRE_INTEGRATE_SSS,          "public\\PreGen\\CS_PreIntegrateSSS.hlsl",     true,  PreIntegrateSSS)\
        PASS(CS_INTEGRATE_SSS_NDF,          "public\\PreGen\\CS_PreIntegrateSSS.hlsl",     true,  IntegrateSSSNDF)\
        PASS(CS_TEMP_SH_Coefficients,       "public\\PreGen\\CS_SHCoefficients.hlsl",      true,  CalcTempSHCoefficients)\
        PASS(CS_SH_Coefficients,            "public\\PreGen\\CS_SHCoefficients.hlsl",      true,  CalcSHCoefficients)

        struct RenderTextureIDs
        {
            static inline size_t PreIntegrateSSSLUTID = PropertyToID(L"Pre Integrate SSS LUT");
            static inline size_t IntegrateSSSNDFLUTID = PropertyToID(L"Integrate SSS NDF LUT");
            static inline size_t PreIntegrateDiffuseID = PropertyToID(L"Pre Integrate Diffuse");
        };

        BakePass();
        virtual ~BakePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            BAKE_PASS_LIST
#undef PASS
            BAKE_PASS_COUNT
        };
        static inline const ShaderPass m_PassData[] =
        {
#define PASS(id, file, isCS, entry) \
{ \
.Name = #id, \
.FilePath = L"Shaders\\" L##file, \
.IsComputeShader = isCS, \
.ComputeEntryPoint = L#entry \
},
            BAKE_PASS_LIST
#undef PASS
        };
#pragma endregion

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        bool m_bIsBakeSHCoefficients = false;
        Vector4 m_SHCoefficientsTempCount;
        EnvironmentData m_GIData{};
        SubsurfaceScatterData m_subsurfaceScatterData{};
        BufferHandle m_pSHCoefficientsTempBuffer = nullptr;

        struct ShaderIDs
        {
            static inline size_t g_PreIntegrateSSSLUTIndex = PropertyToID(L"g_PreIntegrateSSSLUTIndex");
            static inline size_t g_IntegrateSSSNDFLUTIndex = PropertyToID(L"g_IntegrateSSSNDFLUTIndex");
            static inline size_t g_EnvironmentTexIndex = PropertyToID(L"g_EnvironmentTexIndex");
            static inline size_t g_SHCoefficientsBufferIndex = PropertyToID(L"g_SHCoefficientsBufferIndex");
            static inline size_t g_SHCoefficientsTempBufferIndex = PropertyToID(L"g_SHCoefficientsTempBufferIndex");

            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SkyboxSize = PropertyToID(L"g_SkyboxSize");
            static inline size_t g_SHCoefficientsTempCount = PropertyToID(L"g_SHCoefficientsTempCount");
        };
        struct alignas(16) SHCoefficientData
        {
            Vector4 SHCoefficients[9]; // 这个 8x8 区域的 9 个系数的局部累加和
            float TotalWeight;         // 这个 8x8 区域的立体角权重之和
        };

        void DoPreIntegrateSSSLUT();
        void DoIntegrateSSSNDFLUT();
        void DoSHCoefficients();
        void DoCalcSHCoefficients();
        void DoCalcTempSHCoefficients();
    };
}