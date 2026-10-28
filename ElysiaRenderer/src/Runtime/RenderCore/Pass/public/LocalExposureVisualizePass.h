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

    // UE AddVisualizeLocalExposurePass for the Fusion method.
    // Replaces the viewport while visualizeLocalExposure is set.
    class LocalExposureVisualizePass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            // Post-tonemap scene, copied before buffer visualization overwrites DisplayRT.
            static inline size_t SceneSnapshotRTID = PropertyToID(L"Local Exposure Scene Snapshot RT");
        };

        LocalExposureVisualizePass();
        virtual ~LocalExposureVisualizePass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

        // Call before any pass that replaces DisplayRT. No-op when the view is off.
        static void CaptureSceneColor(DX12GraphicsContext* pCommand, RenderTexture* pDisplayRT);

    private:
#define LOCAL_EXPOSURE_VISUALIZE_PASS_LIST(X) \
        X(VISUALIZE_PASS, "public\\PostProcess\\LocalExposure\\CS_LocalExposureVisualize.hlsl", true, VisualizeLocalExposure)
        DECLARE_SHADER_PASSES(LOCAL_EXPOSURE_VISUALIZE_PASS_LIST, LOCAL_EXPOSURE_VISUALIZE_PASS_COUNT);
#undef LOCAL_EXPOSURE_VISUALIZE_PASS_LIST

        struct ShaderIDs
        {
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
            static inline size_t g_DestTexIndex = PropertyToID(L"g_DestTexIndex");
            static inline size_t g_HdrTexIndex = PropertyToID(L"g_HdrTexIndex");
            static inline size_t g_SceneTexIndex = PropertyToID(L"g_SceneTexIndex");
            static inline size_t g_LumTexIndex = PropertyToID(L"g_LumTexIndex");
            static inline size_t g_WeightTexIndex = PropertyToID(L"g_WeightTexIndex");
            static inline size_t g_FusionTexIndex = PropertyToID(L"g_FusionTexIndex");
            static inline size_t g_ExposureScaleBufferIndex = PropertyToID(L"g_ExposureScaleBufferIndex");
            static inline size_t g_PreExposureScaleBufferIndex = PropertyToID(L"g_PreExposureScaleBufferIndex");
            static inline size_t g_Mode = PropertyToID(L"g_Mode");
            static inline size_t g_AutoExposureEnable = PropertyToID(L"g_AutoExposureEnable");
            static inline size_t g_PreExposureEnable = PropertyToID(L"g_PreExposureEnable");
            static inline size_t g_HasFusion = PropertyToID(L"g_HasFusion");
            static inline size_t g_MiddleGrey = PropertyToID(L"g_MiddleGrey");
            static inline size_t g_HighlightThreshold = PropertyToID(L"g_HighlightThreshold");
            static inline size_t g_ShadowThreshold = PropertyToID(L"g_ShadowThreshold");
        };

        void DoVisualize();
    };
}
