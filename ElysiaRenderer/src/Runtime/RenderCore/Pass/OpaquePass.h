#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

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
            static inline int OpaqueLightPassID = -1;
        };
        struct ShaderIDs
        {
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");

            static inline size_t g_AOIndex = PropertyToID(L"g_AOIndex");
        };

        DXGI_FORMAT m_cameraColorFormat = DXGI_FORMAT_UNKNOWN;

        void UpdateLightingPassVariant(UINT passID);
        void DrawLightingPass(ElysiaEngine::FrameContext& context);
    };
}