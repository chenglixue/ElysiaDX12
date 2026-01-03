#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
    class RenderTexture;

    class OpaquePass : public BasePass
    {
    public:
        OpaquePass();
        virtual ~OpaquePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
        struct ShaderPassIDs
        {
            static int OpaqueLightPassID;
        };
        struct ShaderIDs
        {
            static size_t g_AOIndex;

            static size_t screenSize;
            static size_t viewMatrix;
            static size_t viewMatrix_I;
            static size_t projMatrix;
            static size_t projMatrix_I;
            static size_t viewProjMatrix;
            static size_t viewProjMatrix_I;
        };

        DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;

        void UpdateLightingPassVariant(UINT passID);
        void DrawLightingPass(ElysiaEngine::FrameContext& context);
    };
}