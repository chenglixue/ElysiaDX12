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
    using namespace ElysiaCore;

    // UE PostProcessVisualizeHDR: histogram overlay + metering HUD after tonemap.
    class AutoExposureVisualizePass : public BasePass
    {
    public:
        AutoExposureVisualizePass();
        virtual ~AutoExposureVisualizePass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
#define AUTO_EXPOSURE_VISUALIZE_PASS_LIST(X) \
        X(VISUALIZE_HDR_PASS, "public\\PostProcess\\AutoExposure\\CS_AutoExposureVisualize.hlsl", true, VisualizeHDR)
        DECLARE_SHADER_PASSES(AUTO_EXPOSURE_VISUALIZE_PASS_LIST, AUTO_EXPOSURE_VISUALIZE_PASS_COUNT);
#undef AUTO_EXPOSURE_VISUALIZE_PASS_LIST

        struct ShaderIDs
        {
            static inline size_t g_DestTexIndex = PropertyToID(L"g_DestTexIndex");
            static inline size_t g_HdrTexIndex = PropertyToID(L"g_HdrTexIndex");
            static inline size_t g_HistogramTexIndex = PropertyToID(L"g_HistogramTexIndex");
            static inline size_t g_ExposureScaleBufferIndex = PropertyToID(L"g_ExposureScaleBufferIndex");
            static inline size_t g_VisualizeDebugType = PropertyToID(L"g_VisualizeDebugType");
            static inline size_t g_LuminanceScale = PropertyToID(L"g_LuminanceScale");
            static inline size_t g_HistogramScale = PropertyToID(L"g_HistogramScale");
            static inline size_t g_HistogramBias = PropertyToID(L"g_HistogramBias");
            static inline size_t g_LowPercent = PropertyToID(L"g_LowPercent");
            static inline size_t g_HighPercent = PropertyToID(L"g_HighPercent");
            static inline size_t g_MinAverageLuminance = PropertyToID(L"g_MinAverageLuminance");
            static inline size_t g_MaxAverageLuminance = PropertyToID(L"g_MaxAverageLuminance");
            static inline size_t g_MiddleGrey = PropertyToID(L"g_MiddleGrey");
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
        };

        void DoVisualizeHDR();
    };
}
