#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
    class DX12Light;
}

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    class ShadowPass : public BasePass
    {
    public:
        struct ShaderPassIDs
        {
            static int ShadowCastPassID;
        };
        struct RenderTextureIDs
        {
            static size_t ShadowRTID;
        };
        struct ShaderIDs
        {
            static size_t shadowNearZ;
            static size_t shadowFarZ;
            static size_t shadowDepthBias;
            static size_t shadowSlopeDepthBias;
            static size_t shadowMaxSlopeDepthBias;
            static size_t g_sobolSequence;
            static size_t worldMatrix;
            static size_t baseColorTexIndex;
            static size_t opacity;
            static size_t cutoff;
        };

    public:
        ShadowPass(DX12Camera* pCamera);
        virtual ~ShadowPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
        DX12Light* m_pMainLight;
        std::vector<Vector2> m_sobolSqeuences;

        void UpdateShadowPassVariant(UINT passIndex);
        void DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex);
        void DrawShadowPass(ElysiaEngine::FrameContext& context);
    };
}