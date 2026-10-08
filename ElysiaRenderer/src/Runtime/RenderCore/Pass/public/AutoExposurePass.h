#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace ElysiaHelper;

    class AutoExposurePass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t AORTID = PropertyToID(L"Auto Exposure RT");
        };

    public:
        AutoExposurePass();
        virtual ~AutoExposurePass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_halfWidth;
        UINT m_halfHeight;
        UINT m_quarterWidth;
        UINT m_quarterHeight;

        RenderTexture* m_pAORT = nullptr;

#define AUTO_EXPOSURE_PASS_LIST(X) \
        X(Deinterleaved_Depth_PASS,          "public\\PostProcess\\AutoExposure\\CS_AutoExposure.hlsl",               true, Main)
        DECLARE_SHADER_PASSES(AUTO_EXPOSURE_PASS_LIST, AUTO_EXPOSURE_PASS_COUNT);
#undef AUTO_EXPOSURE_PASS_LIST

    };
}