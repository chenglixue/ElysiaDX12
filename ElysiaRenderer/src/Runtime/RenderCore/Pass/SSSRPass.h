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
    PASS(BLOOM_FIRST_DOWN_SAMPLE_PASS,          "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomKarisDownSample)

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

        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_displayWidth;
        UINT m_displayHeight;
        BufferHandle m_pRayCounterBuffer = nullptr;
        BufferHandle m_pIntersectionIndirectArgs = nullptr;

        struct ShaderIDs
        {
            static inline size_t g_DestTextureIndexID = PropertyToID(L"g_DestTextureIndex");

        };

        void DoTileClassify();
    };
}