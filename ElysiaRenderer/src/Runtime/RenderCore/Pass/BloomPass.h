#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
#define BLOOM_PASS_LIST \
    PASS(BLOOM_FIRST_DOWN_SAMPLE_PASS,          "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomKarisDownSample)\
    PASS(BLOOM_WEIGHT_DOWN_SAMPLE_PASS,         "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomWeightedDownSample)\
    PASS(BLOOM_3X3TENT_UP_SAMPLE,               "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  Bloom3x3TentUpSample)\
    PASS(BLOOM_BLEND_SCENE_COLOR,               "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomBlendSceneColor)

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
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            BLOOM_PASS_LIST
#undef PASS
            BLOOM_PASS_COUNT
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
            BLOOM_PASS_LIST
#undef PASS
        };
#pragma endregion

        UINT m_cameraWidth;
        UINT m_cameraHeight;
        static const UINT m_mipmapCount = 6;
        RenderTexture* m_pBloomRT = nullptr;
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
        void DoBloom3x3TentUpSample();
        void DoBloomBlendSceneColor();
    };
}