#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class SSGIPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t RawRTID = PropertyToID(L"SSGI Raw RT");
            static inline size_t IndirectRTID = PropertyToID(L"SSGI Indirect RT");
            static inline size_t RadianceRTID = PropertyToID(L"SSGI Radiance History RT");
        };

        SSGIPass();
        ~SSGIPass() override;

        void Configure() override;
        void Render(ElysiaEngine::FrameContext& context) override;
        void UpdatePipeline() override;
        void Dispose() override;

    private:
#define SSGI_PASS_LIST(X) \
        X(SSGI_TRACE_PASS, "public\\PostProcess\\SSGI\\CS_SSGI.hlsl", true, Trace) \
        X(SSGI_TEMPORAL_PASS, "public\\PostProcess\\SSGI\\CS_SSGI.hlsl", true, Temporal) \
        X(SSGI_COMPOSITE_PASS, "public\\PostProcess\\SSGI\\CS_SSGI.hlsl", true, Composite) \
        X(SSGI_COPY_HISTORY_PASS, "public\\PostProcess\\SSGI\\CS_SSGI.hlsl", true, CopyHistory)
        DECLARE_SHADER_PASSES(SSGI_PASS_LIST, SSGI_PASS_COUNT);
#undef SSGI_PASS_LIST

        struct ShaderIDs
        {
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t g_FullSize = PropertyToID(L"g_FullSize");
            static inline size_t g_HalfSize = PropertyToID(L"g_HalfSize");
            static inline size_t g_MaxDistance = PropertyToID(L"g_MaxDistance");
            static inline size_t g_Thickness = PropertyToID(L"g_Thickness");
            static inline size_t g_SelfBias = PropertyToID(L"g_SelfBias");
            static inline size_t g_Intensity = PropertyToID(L"g_Intensity");
            static inline size_t g_HistoryWeight = PropertyToID(L"g_HistoryWeight");
            static inline size_t g_FrameIndex = PropertyToID(L"g_FrameIndex");
            static inline size_t g_RadianceHistorySRV = PropertyToID(L"g_RadianceHistorySRV");
            static inline size_t g_RadianceHistoryUAV = PropertyToID(L"g_RadianceHistoryUAV");
            static inline size_t g_RawIndirectSRV = PropertyToID(L"g_RawIndirectSRV");
            static inline size_t g_RawIndirectUAV = PropertyToID(L"g_RawIndirectUAV");
            static inline size_t g_IndirectHistorySRV = PropertyToID(L"g_IndirectHistorySRV");
            static inline size_t g_IndirectDestSRV = PropertyToID(L"g_IndirectDestSRV");
            static inline size_t g_IndirectDestUAV = PropertyToID(L"g_IndirectDestUAV");
            static inline size_t g_CameraColorSRV = PropertyToID(L"g_CameraColorSRV");
            static inline size_t g_CameraColorUAV = PropertyToID(L"g_CameraColorUAV");
            static inline size_t g_PreExposureBufferIndex = PropertyToID(L"g_PreExposureBufferIndex");
            static inline size_t g_PreExposureEnable = PropertyToID(L"g_PreExposureEnable");
            static inline size_t g_HasRadianceHistory = PropertyToID(L"g_HasRadianceHistory");
            static inline size_t g_HasIndirectHistory = PropertyToID(L"g_HasIndirectHistory");
        };

        UINT m_fullWidth = 0;
        UINT m_fullHeight = 0;
        UINT m_halfWidth = 0;
        UINT m_halfHeight = 0;
        RenderTexture* m_pRawRT = nullptr;
        RenderTexture* m_pIndirectRT[2] = {};
        RenderTexture* m_pRadianceHistoryRT = nullptr;
        UINT m_indirectRead = 0;
        bool m_radianceValid = false;
        bool m_indirectValid = false;

        void BindFrame(PassID passID);
        void Dispatch(PassID passID, UINT width, UINT height);
    };
}
