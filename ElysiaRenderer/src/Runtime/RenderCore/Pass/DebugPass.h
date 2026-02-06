#pragma once
#include "BasePass.h"
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
            static size_t g_TargetSize;
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
        };

        void DoDebugPass();
    };
}