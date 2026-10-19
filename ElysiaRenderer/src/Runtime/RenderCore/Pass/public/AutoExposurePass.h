#pragma once
#include "BasePass.h"
#include "Runtime/Core/public/BufferUtility.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;
    struct AutoExposureData;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace ElysiaHelper;
    using namespace ElysiaCore;

    class AutoExposurePass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t AutoExposureScaleRTID = PropertyToID(L"Auto Exposure Scale RT");
            static inline size_t AutoExposurePreScaleRTID = PropertyToID(L"Auto Exposure Pre Scale RT");
            static inline size_t AutoExposureHistogramRTID = PropertyToID(L"Auto Exposure Histogram RT");
            static inline size_t AutoExposureHdrSnapshotRTID = PropertyToID(L"Auto Exposure HDR Snapshot RT");
        };

    public:
        AutoExposurePass();
        virtual ~AutoExposurePass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

        // Shared with Tonemap / VisualizeHDR. Bindless SRV via GetResourceHeapIndex().
        static inline BufferHandle m_pExposureScaleBuffer;

        BufferHandle GetExposureScaleBuffer() const { return m_pExposureScaleBuffer; }

    private:
        UINT m_displayWidth = 0;
        UINT m_displayHeight = 0;
        float m_deltaTime = 1.0f / 60.0f;
        bool m_snapExposure = true;

        BufferHandle m_pHistogramBuffer;
        BufferHandle m_pPreExposureBuffer;
        RenderTexture* m_pHistogramRT = nullptr;
        RenderTexture* m_pHDRSnapshotRT = nullptr;
        AutoExposureData m_autoExposureData{};

#define AUTO_EXPOSURE_PASS_LIST(X) \
        X(CLEAR_HISTOGRAM_PASS,  "public\\PostProcess\\AutoExposure\\CS_AutoExposure.hlsl", true, ClearHistogram) \
        X(BUILD_HISTOGRAM_PASS,  "public\\PostProcess\\AutoExposure\\CS_AutoExposure.hlsl", true, BuildHistogram) \
        X(COMPUTE_EXPOSURE_PASS, "public\\PostProcess\\AutoExposure\\CS_AutoExposure.hlsl", true, ComputeExposure) \
        X(RESET_PRE_EXPOSURE_PASS, "public\\PostProcess\\AutoExposure\\CS_AutoExposure.hlsl", true, ResetPreExposure)
        DECLARE_SHADER_PASSES(AUTO_EXPOSURE_PASS_LIST, AUTO_EXPOSURE_PASS_COUNT);
#undef AUTO_EXPOSURE_PASS_LIST

        struct ShaderIDs
        {
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_HistogramBufferIndex = PropertyToID(L"g_HistogramBufferIndex");
            static inline size_t g_ExposureBufferScaleIndex = PropertyToID(L"g_ExposureScaleBufferIndex");
            static inline size_t g_PreExposureScaleBufferIndex = PropertyToID(L"g_PreExposureScaleBufferIndex");
            static inline size_t g_HistogramTexIndex = PropertyToID(L"g_HistogramTexIndex");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_HistogramScale = PropertyToID(L"g_HistogramScale");
            static inline size_t g_HistogramBias = PropertyToID(L"g_HistogramBias");
            static inline size_t g_LowPercent = PropertyToID(L"g_LowPercent");
            static inline size_t g_HighPercent = PropertyToID(L"g_HighPercent");
            static inline size_t g_MinAverageLuminance = PropertyToID(L"g_MinAverageLuminance");
            static inline size_t g_MaxAverageLuminance = PropertyToID(L"g_MaxAverageLuminance");
            static inline size_t g_MiddleGrey = PropertyToID(L"g_MiddleGrey");
            static inline size_t g_ExposureCompensationEV = PropertyToID(L"g_ExposureCompensationEV");
            static inline size_t g_ExposureSpeedUp = PropertyToID(L"g_ExposureSpeedUp");
            static inline size_t g_ExposureSpeedDown = PropertyToID(L"g_ExposureSpeedDown");
            static inline size_t g_ExponentialTransitionDistance = PropertyToID(L"g_ExponentialTransitionDistance");
            static inline size_t g_DeltaTime = PropertyToID(L"g_DeltaTime");
            static inline size_t g_PreExposureEnable = PropertyToID(L"g_PreExposureEnable");
            static inline size_t g_ForceTarget = PropertyToID(L"g_ForceTarget");
        };

        void BindCommonParams(PassID passID);
        void DoClearHistogram();
        void DoBuildHistogram();
        void DoComputeExposure();
        void DoResetPreExposure();
        void DoSnapshotHdr();
    };
}
