#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/AOUtility.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace CAULDRON_DX12;
    using namespace ElysiaHelper;

    class AOPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t AORTID = SIZE_MAX;
            static inline size_t HalfAORTID = SIZE_MAX;
            static inline size_t OneFourAORTID = SIZE_MAX;
            static inline size_t HIZRTID = SIZE_MAX;
        };

    public:
        AOPass();
        virtual ~AOPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
        UINT m_HIZMipmapCount;

        RenderTexture* m_pAORT = nullptr;
        RenderTexture* m_pHalfAORT = nullptr;
        RenderTexture* m_pOneFourAORT = nullptr;
        std::vector<RenderTexture*> m_depthTempRTs;
        RenderTexture* m_pHIZRT = nullptr;
        RenderTexture* m_pRandStepRT = nullptr;
        TextureManager::Handle m_blueNoise;

        struct ShaderPasseIDs
        {
            static inline int AOPassID = -1;
            static inline int HIZPassID = -1;
        };
        struct ShaderIDs
        {
            static size_t g_TargetSize;
            static size_t g_SourceSize;
            static size_t g_TargetTexIndex;
            static size_t g_SourceTexIndex;
            static size_t g_TargetMipmapLevel;
            static size_t viewMatrix;
            static size_t viewMatrix_I;
            static size_t projMatrix;
            static size_t projMatrix_I;
            static size_t viewProjMatrix;
            static size_t viewProjMatrix_I;

            static size_t g_AOSampleKernelArray;
            static size_t g_AOSampleCount;
            static size_t g_AOSampleStepCount;
            static size_t g_AORadius;
            static size_t g_AOFadeRadius;
            static size_t g_AOFadeDistance;
            static size_t g_AOBias;
            static size_t g_AOIntensityMul;
            static size_t g_AOIntensityPow;
            static size_t g_bLerpAO;

            static size_t g_AOIndex;
            static inline size_t g_RandStepTexIndex = PropertyToID(L"g_RandStepTexIndex");
            static size_t g_noiseScale;

            static size_t g_HIZMaxMipmap;
            static size_t g_HIZMinMipmap;
            static size_t g_MipmapLevel;
            static inline size_t g_StepMipFactor = PropertyToID(L"g_StepMipFactor");
            static size_t g_HIZTextureIndex;
            static size_t g_IsNormal;

            static size_t g_BlurDir;
            static size_t g_Sharpness;
            static size_t g_BlurRadius;
            static size_t g_Weights;
            static size_t g_BlurIntensity;
        };

        std::vector<Vector4> m_kernels;
        std::vector<UINT> m_randSteps;

        void DoSSAO();
        void DoHIZ();
        std::vector<Vector4> GenerateSSAOSampleKernel();
        std::vector<Vector4> GenerateHBAOSampleKernel();
        std::vector<UINT> GenerateRandStepData();
        std::vector<float> GenerateBlurWeights(UINT blurRadius, float sigma);
        DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
    };
}