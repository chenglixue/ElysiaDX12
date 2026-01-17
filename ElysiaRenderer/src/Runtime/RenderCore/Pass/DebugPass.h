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

    class DebugPass : public BasePass
    {
    public:
        DebugPass();
        virtual ~DebugPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
        struct ShaderPasseIDs
        {
            static int DebugPassID;
        };

        struct ShaderIDs
        {
            static size_t g_DebugMode;
            static size_t g_TargetTexIndex;
            static size_t g_SourceTexIndex;
            static size_t g_MipmapLevel;
            static size_t g_SourceSize;
        };

        void DoDebugPass();
    };
}