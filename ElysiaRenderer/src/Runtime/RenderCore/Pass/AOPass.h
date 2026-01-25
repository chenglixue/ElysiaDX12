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
            static inline size_t AORTID = PropertyToID(L"AO RT");
            static inline size_t HalfAORTID = PropertyToID(L"Half AO RT");
            static inline size_t OneFourAORTID = PropertyToID(L"One Four AO RT");
            static inline size_t HIZRTID = PropertyToID(L"AO HIZ RT");
            static inline size_t TAA0RTID = PropertyToID(L"AO TAA0 RT");
            static inline size_t TAA1RTID = PropertyToID(L"AO TAA1 RT");
            static inline size_t AOBlurHorizonRTID = PropertyToID(L"AO Blur Horizon RT");
            static inline size_t AOBlurVerticalRTID = PropertyToID(L"AO Blur Vertical RT");
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
        static inline bool m_isFirstFrame = true;
        int m_currHistoryIndex = 0;
        static constexpr UINT MAX_BLUR_RADIUS = 10;

        RenderTexture* m_pAORT = nullptr;
        RenderTexture* m_pHalfAORT = nullptr;
        RenderTexture* m_pOneFourAORT = nullptr;
        RenderTexture* m_pHIZRT = nullptr;
        RenderTexture* m_pTAA0RT = nullptr;
        RenderTexture* m_pTAA1RT = nullptr;
        RenderTexture* m_pBlurHorizonRT = nullptr;
        RenderTexture* m_pBlurVerticalRT = nullptr;
        TextureManager::Handle m_blueNoise;

        struct ShaderPasseIDs
        {
            static inline int HIZPassID = -1;
            static inline int AOPassID = -1;
            static inline int TAAPassID = -1;
            static inline int BlurHorizonPassID = -1;
            static inline int BlurVerticalPassID = -1;
        };
        struct ShaderIDs
        {
            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_TargetMipmapLevel = PropertyToID(L"g_TargetMipmapLevel");

            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t g_ProjectScale = PropertyToID(L"g_ProjectScale");

            static inline size_t pre_viewMatrix = PropertyToID(L"pre_viewMatrix");
            static inline size_t pre_viewMatrix_I = PropertyToID(L"pre_viewMatrix_I");
            static inline size_t pre_projMatrix = PropertyToID(L"pre_projMatrix");
            static inline size_t pre_projMatrix_I = PropertyToID(L"pre_projMatrix_I");
            static inline size_t pre_viewProjMatrix = PropertyToID(L"pre_viewProjMatrix");
            static inline size_t pre_viewProjMatrix_I = PropertyToID(L"pre_viewProjMatrix_I");

            static inline size_t g_AOSampleKernelArray = PropertyToID(L"g_AOSampleKernelArray");
            static inline size_t g_AOSampleCount = PropertyToID(L"g_AOSampleCount");
            static inline size_t g_AOSampleStepCount = PropertyToID(L"g_AOSampleStepCount");
            static inline size_t g_AORadius = PropertyToID(L"g_AORadius");
            static inline size_t g_AOFadeRadius = PropertyToID(L"g_AOFadeRadius");
            static inline size_t g_AOFadeDistance = PropertyToID(L"g_AOFadeDistance");
            static inline size_t g_AOBias = PropertyToID(L"g_AOBias");
            static inline size_t g_AOIntensityMul = PropertyToID(L"g_AOIntensityMul");
            static inline size_t g_AOIntensityPow = PropertyToID(L"g_AOIntensityPow");
            static inline size_t g_bLerpAO = PropertyToID(L"g_bLerpAO");
            static inline size_t g_LerpAOFactor = PropertyToID(L"g_LerpAOFactor");
            static inline size_t g_BlendWeight = PropertyToID(L"g_BlendWeight");

            static inline size_t g_AOIndex = PropertyToID(L"g_AOIndex");
            static inline size_t g_RandStepTexIndex = PropertyToID(L"g_RandStepTexIndex");
            static inline size_t g_noiseScale = PropertyToID(L"g_noiseScale");

            static inline size_t g_HIZMaxMipmap = PropertyToID(L"g_HIZMaxMipmap");
            static inline size_t g_HIZMinMipmap = PropertyToID(L"g_HIZMinMipmap");
            static inline size_t g_MipmapLevel = PropertyToID(L"g_MipmapLevel");
            static inline size_t g_StepMipFactor = PropertyToID(L"g_StepMipFactor");
            static inline size_t g_HIZTextureIndex = PropertyToID(L"g_HIZTextureIndex");

            static inline size_t g_BlurDir = PropertyToID(L"g_BlurDir");
            static inline size_t g_Sharpness = PropertyToID(L"g_Sharpness");
            static inline size_t g_BlurRadius = PropertyToID(L"g_BlurRadius");
            static inline size_t g_Weights = PropertyToID(L"g_Weights");
            static inline size_t g_BlurIntensity = PropertyToID(L"g_BlurIntensity");
        };
        struct TAAData
        {
            static inline Matrix Pre_View_M;
            static inline Matrix Pre_Proj_M;
            static inline Matrix Pre_ViewProj_M;
            static inline Matrix Pre_View_I_M;
            static inline Matrix Pre_Proj_I_M;
            static inline Matrix Pre_ViewProj_I_M;
        };

        std::vector<Vector4> m_kernels;
        std::vector<float> m_blurWeights;

        void DoSSAO();
        void DoHIZ();
        void DoTAA();
        void DoBilateralBlurHorizon();
        void DoBilateralBlurVerical();
        std::vector<Vector4> GenerateSSAOSampleKernel();
        std::vector<Vector4> GenerateHBAOSampleKernel();
        std::vector<float> GenerateBlurWeights(UINT blurRadius);
    };
}