#pragma once
#include "BasePass.h"


namespace ElysiaRenderer
{
    class RenderTexture;

    class FinalBlitPass : public BasePass
    {
    public:
        FinalBlitPass() = default;
        FinalBlitPass(DX12Camera* pCamera);
        virtual ~FinalBlitPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
        struct ShaderPassIDs
        {
            static int BlitPassID;
        };
        struct ShaderIDs
        {
            static size_t blitterTextureIndex;
        };

        DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_UNKNOWN;
        void UpdateFinalBlitVariant(UINT passID);
        void DoFinalBlit();
    };
}