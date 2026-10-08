#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class SharpenPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t SharpenRTID = PropertyToID(L"Sharpen RT");
        };

    public:
        SharpenPass();
        virtual ~SharpenPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#define SHARPEN_PASS_LIST(X) \
        X(CAS_PASS,          "public\\PostProcess\\Sharpen\\CS_CAS.hlsl",               true,  CAS)
        DECLARE_SHADER_PASSES(SHARPEN_PASS_LIST, SHARPEN_PASS_COUNT);
#undef SHARPEN_PASS_LIST
        UINT m_renderWidth;
        UINT m_renderHeight;

        struct ShaderIDs
        {
            static inline size_t g_SharpenTexSize = PropertyToID(L"g_SharpenTexSize");
            static inline size_t g_SharpenTexIndex = PropertyToID(L"g_SharpenTexIndex");
            static inline size_t g_SharpenIntensity = PropertyToID(L"g_SharpenIntensity");
        };
        void DoCAS();
    };
}