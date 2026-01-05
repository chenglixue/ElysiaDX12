#pragma once
#include "BasePass.h"

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
        RenderTexture* m_pAORT = nullptr;
        TextureManager::Handle m_blueNoise;

        struct ShaderPasseIDs
        {
            static int AOPassID;
        };

        struct ShaderIDs
        {
            static size_t g_DestSize;
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
        };

        std::vector<Vector4> m_kernels;

        void DoCalcAO();
        void DoBlitToBackBuffer();
        std::vector<Vector4> GenerateSSAOSampleKernel();
        DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;
    };
}