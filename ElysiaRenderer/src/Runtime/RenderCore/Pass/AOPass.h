#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/AOUtility.h"

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
            static size_t AORTID;
            static size_t BlurHorizionRTID;
            static size_t BlurVerticalRTID;
            static size_t HIZRTID;
        };

    public:
        AOPass();
        virtual ~AOPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

        void UpdateGBufferPassVariant(UINT passIndex);

    private:
        static const UINT m_maxHIZCount = 10;
        UINT m_mipmapCount;

        RenderTexture* m_pAORT = nullptr;
        RenderTexture* m_pBlurHorizionRT = nullptr;
        RenderTexture* m_pBlurVerticalRT = nullptr;
        std::vector<RenderTexture*> m_pTempRTs;
        RenderTexture* m_pHIZRT;
        TextureManager::Handle m_blueNoise;

        struct ShaderPasseIDs
        {
            static int AOPassID;
            static int BlurHorizionPassID;
            static int BlurVerticalPassID;
            static int HIZPassID;
            static int CopyTexturePassID;
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
            static size_t g_AORadius;
            static size_t g_AOBias;
            static size_t g_AOIntensityMul;
            static size_t g_AOIntensityPow;

            static size_t g_AOIndex;
            static size_t g_noiseScale;

            static size_t g_BlurDir;
            static size_t g_Sharpness;
            static size_t g_BlurRadius;
            static size_t g_Weights;
            static size_t g_BlurIntensity;
        };

        std::vector<Vector4> m_kernels;
        std::vector<float> m_blurWeights;
        AOBlurQuality m_lastBlurQuality;
        const UINT m_blurRadius = 4;
        const float m_blurSigma = 2;

        void DoSSAO();
        void DoBlurHorizion();
        void DoBlurVertical();
        void DoHIZ();
        std::vector<Vector4> GenerateSSAOSampleKernel();
        std::vector<float> GenerateBlurWeights(UINT blurRadius, float sigma);
        DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
    };
}