#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class BloomPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t BloomRTID = PropertyToID(L"Bloom RT");
            static inline size_t BloomDownSampleRTID = PropertyToID(L"Bloom Down Sample RT");
            static inline size_t BloomUpSampleRTID = PropertyToID(L"Bloom Up Sample RT");
        };

    public:
        BloomPass();
        virtual ~BloomPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#define BLOOM_PASS_LIST(X) \
        X(BLOOM_FIRST_DOWN_SAMPLE_PASS,          "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomKarisDownSample) \
        X(BLOOM_WEIGHT_DOWN_SAMPLE_PASS,         "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomWeightedDownSample) \
        X(BLOOM_3X3TENT_UP_SAMPLE,               "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  Bloom3x3TentUpSample) \
        X(BLOOM_BLEND_SCENE_COLOR,               "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomBlendSceneColor) \
        X(COPY_RT,                               "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  CopyRT)
        DECLARE_SHADER_PASSES(BLOOM_PASS_LIST, BLOOM_PASS_COUNT);
#undef BLOOM_PASS_LIST

        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_displayWidth;
        UINT m_displayHeight;
        static const UINT m_mipmapCount = 6;
        std::array<UINT2, m_mipmapCount> m_mipmapResolutions{};
        std::array<RenderTexture*, m_mipmapCount> m_downSampleRTs{};
        std::array<RenderTexture*, m_mipmapCount> m_upSampleRTs{};

        struct ShaderIDs
        {
            static inline size_t g_DestTextureIndexID = PropertyToID(L"g_DestTextureIndex");
            static inline size_t g_SourceTextureIndex = PropertyToID(L"g_SourceTextureIndex");
            static inline size_t g_DownSampleDestTexIndex = PropertyToID(L"g_DownSampleDestTexIndex");
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_BloomRadius = PropertyToID(L"g_BloomRadius");
            static inline size_t g_BloomIntensity = PropertyToID(L"g_BloomIntensity");
            static inline size_t g_TargetMipLevel = PropertyToID(L"g_TargetMipLevel");
        };

        void DoBloomFirstDownSample();
        void DoBloomWeightDownSample();
        void DoCopyLastDownSampleRT2LastUpSampleRT();
        void DoBloom3x3TentUpSample();
    };
}