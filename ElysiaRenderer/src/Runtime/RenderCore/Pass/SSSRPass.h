#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class SSSRPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t BloomRTID = PropertyToID(L"Bloom RT");
        };

        SSSRPass();
        virtual ~SSSRPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#define SSSR_PASS_LIST \
    PASS(SSSR_TILE_CLASSIFY_PASS,           "public\\PostProcess\\SSSR\\CS_TileClassify.hlsl",              true,  TileClassify)\
    PASS(SSSR_INTERSECT_ARGS_PASS,          "public\\PostProcess\\SSSR\\CS_TileClassify.hlsl",              true,  DoIntersectArgs)\
    PASS(SSSR_INTERSECT_PASS,               "public\\PostProcess\\SSSR\\CS_Intersect.hlsl",                 true,  DoIntersect)

#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            SSSR_PASS_LIST
#undef PASS
            SSSR_PASS_COUNT
        };

        static inline const ShaderPass m_PassData[] =
        {
#define PASS(id, file, isCS, entry) \
{ \
.Name = #id, \
.FilePath = L"Shaders\\" L##file, \
.IsComputeShader = isCS, \
.ComputeEntryPoint = L#entry \
},
            SSSR_PASS_LIST
#undef PASS
        };
#pragma endregion

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        BufferHandle m_pRayCounterBuffer = nullptr;
        BufferHandle m_pIntersectionIndirectArgsBuffer = nullptr;
        BufferHandle m_pRayListBuffer = nullptr;
        RenderTexture* m_pIntersectionOutputRT = nullptr;

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

            static inline size_t g_RayCounterBufferIndex = PropertyToID(L"g_RayCounterBufferIndex");
            static inline size_t g_RayListBufferIndex = PropertyToID(L"g_RayListBufferIndex");
            static inline size_t g_IntersectionOutputTexIndex = PropertyToID(L"g_IntersectionOutputTexIndex");
            static inline size_t g_IntersectionArgsBufferIndex = PropertyToID(L"g_IntersectionArgsBufferIndex");

            static inline size_t g_DestTextureIndexID = PropertyToID(L"g_DestTextureIndex");
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
            static inline size_t g_RoughnessThreshold = PropertyToID(L"g_RoughnessThreshold");
            static inline size_t g_SamplesPerQuad = PropertyToID(L"g_SamplesPerQuad");
        };

        void DoTileClassify();
        void DoTileClassifyDebug();
        void DoIntersectionArgs();
        void DoIntersection();
    };
}