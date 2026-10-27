#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class LocalExposureFusionPass : public BasePass
    {
    public:
        static const UINT kLevelCount = 6;

        struct RenderTextureIDs
        {
            static inline size_t LumRTID = PropertyToID(L"Local Exposure Fusion Lum RT");
            static inline size_t WeightRTID = PropertyToID(L"Local Exposure Fusion Weight RT");
            static inline size_t ResultRTID = PropertyToID(L"Local Exposure Fusion Result RT");
        };

        LocalExposureFusionPass();
        virtual ~LocalExposureFusionPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

        UINT GetLevelCount() const;
        RenderTexture* GetResultRT() const;

    private:
#define LOCAL_EXPOSURE_FUSION_PASS_LIST(X) \
        X(FUSION_SETUP_PASS, "public\\PostProcess\\LocalExposure\\CS_LocalExposureFusion.hlsl", true, FusionSetup) \
        X(FUSION_DOWNSAMPLE_PASS, "public\\PostProcess\\LocalExposure\\CS_LocalExposureFusion.hlsl", true, FusionDownsample) \
        X(FUSION_BLEND_PASS, "public\\PostProcess\\LocalExposure\\CS_LocalExposureFusion.hlsl", true, FusionBlend)
        DECLARE_SHADER_PASSES(LOCAL_EXPOSURE_FUSION_PASS_LIST, LOCAL_EXPOSURE_FUSION_PASS_COUNT);
#undef LOCAL_EXPOSURE_FUSION_PASS_LIST

        struct ShaderIDs
        {
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_LumTexIndex = PropertyToID(L"g_LumTexIndex");
            static inline size_t g_WeightTexIndex = PropertyToID(L"g_WeightTexIndex");
            static inline size_t g_CoarseLumTexIndex = PropertyToID(L"g_CoarseLumTexIndex");
            static inline size_t g_CoarseResultTexIndex = PropertyToID(L"g_CoarseResultTexIndex");
            static inline size_t g_DestLumTexIndex = PropertyToID(L"g_DestLumTexIndex");
            static inline size_t g_DestWeightTexIndex = PropertyToID(L"g_DestWeightTexIndex");
            static inline size_t g_DestResultTexIndex = PropertyToID(L"g_DestResultTexIndex");
            static inline size_t g_ExposureScaleBufferIndex = PropertyToID(L"g_ExposureScaleBufferIndex");
            static inline size_t g_PreExposureScaleBufferIndex = PropertyToID(L"g_PreExposureScaleBufferIndex");
            static inline size_t g_AutoExposureEnable = PropertyToID(L"g_AutoExposureEnable");
            static inline size_t g_PreExposureEnable = PropertyToID(L"g_PreExposureEnable");
            static inline size_t g_IsCoarsest = PropertyToID(L"g_IsCoarsest");
            static inline size_t g_HighlightContrast = PropertyToID(L"g_HighlightContrast");
            static inline size_t g_ShadowContrast = PropertyToID(L"g_ShadowContrast");
            static inline size_t g_TargetLuminance = PropertyToID(L"g_TargetLuminance");
        };

        UINT m_levelCount = 0;
        std::array<UINT2, kLevelCount> m_levelSize{};
        std::array<RenderTexture*, kLevelCount> m_lumRTs{};
        std::array<RenderTexture*, kLevelCount> m_weightRTs{};
        std::array<RenderTexture*, kLevelCount> m_resultRTs{};

        void DispatchSetup();
        void DispatchDownsample(UINT level);
        void DispatchBlend(UINT level);
    };
}