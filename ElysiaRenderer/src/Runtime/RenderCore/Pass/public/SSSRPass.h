#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class SSSRPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t BloomRTID = PropertyToID(L"Bloom RT");
            static inline size_t HIZRTID = PropertyToID(L"HIZ RT");
            static inline size_t TileClassifyDebugRTID = PropertyToID(L"SSSR Tile Classify Debug RT");
        };

        SSSRPass();
        virtual ~SSSRPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#define SSSR_PASS_LIST(X) \
        X(SSSR_COPY_PASS,                    "public\\PostProcess\\SSSR\\CS_SPD.hlsl",                       true,  DoCopyRT) \
        X(SSSR_SPD_PASS,                     "public\\PostProcess\\SSSR\\CS_SPD.hlsl",                       true,  DoSPD) \
        X(SSSR_TILE_CLASSIFY_PASS,           "public\\PostProcess\\SSSR\\CS_TileClassify.hlsl",              true,  DoTileClassify) \
        X(SSSR_INTERSECT_ARGS_PASS,          "public\\PostProcess\\SSSR\\CS_TileClassify.hlsl",              true,  DoIntersectArgs) \
        X(SSSR_INTERSECT_PASS,               "public\\PostProcess\\SSSR\\CS_Intersect.hlsl",                 true,  DoIntersect)
        DECLARE_SHADER_PASSES(SSSR_PASS_LIST, SSSR_PASS_COUNT);
#undef SSSR_PASS_LIST

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_HIZWidth;
        UINT m_HIZHeight;
        
        UINT m_HIZMipmapCount;
        
        RenderTexture* m_pHIZRT = nullptr;
        BufferHandle m_pRayCounterBuffer = nullptr;
        BufferHandle m_pIntersectionIndirectArgsBuffer = nullptr;
        BufferHandle m_pRayListBuffer = nullptr;
        RenderTexture* m_pIntersectionOutputRT = nullptr;
        RenderTexture* m_pTileClassifyDebugRT = nullptr;

        BufferHandle m_pRayCounterReadBackBuffer;
        BufferHandle m_pIntersectionArgsReadBackBuffer;

        ComPtr<ID3D12CommandSignature> m_pCommandSignature;

        struct ShaderIDs
        {
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");

            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_HIZTexIndex = PropertyToID(L"g_HIZTexIndex");
            static inline size_t g_RayCounterBufferIndex = PropertyToID(L"g_RayCounterBufferIndex");
            static inline size_t g_RayListBufferIndex = PropertyToID(L"g_RayListBufferIndex");
            static inline size_t g_IntersectionOutputTexIndex = PropertyToID(L"g_IntersectionOutputTexIndex");
            static inline size_t g_TileClassifyDebugTexIndex = PropertyToID(L"g_TileClassifyDebugTexIndex");
            static inline size_t g_TileClassifyDebugView = PropertyToID(L"g_TileClassifyDebugView");
            static inline size_t g_IntersectionArgsBufferIndex = PropertyToID(L"g_IntersectionArgsBufferIndex");

            static inline size_t g_DestTextureIndexID = PropertyToID(L"g_DestTextureIndex");
            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_RoughnessThreshold = PropertyToID(L"g_RoughnessThreshold");
            static inline size_t g_SamplesPerQuad = PropertyToID(L"g_SamplesPerQuad");
        };

        void DoSPD();
        void DoTileClassify();
        void DoTileClassifyDebug();
        void DoIntersectionArgs();
        void DoIntersection();
        void BindCommon(UINT PassID);
    };
}
