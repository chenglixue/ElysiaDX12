#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
#define TAA_PASS_LIST \
PASS(BLOOM_FIRST_DOWN_SAMPLE_PASS,          "public\\PostProcess\\Bloom\\Bloom.hlsl",               true,  BloomKarisDownSample)

    class TAAPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t TAA0RTID = PropertyToID(L"TAA 0 RT");
            static inline size_t TAA1RTID = PropertyToID(L"TAA 1 RT");

        };

    public:
        TAAPass();
        virtual ~TAAPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            TAA_PASS_LIST
#undef PASS
            TAA_PASS_COUNT
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
            TAA_PASS_LIST
#undef PASS
        };
#pragma endregion
        UINT m_cameraWidth;
        UINT m_cameraHeight;

        int m_writeIndex = 0;
        const static UINT m_TAARTCount = 2;
        std::array<RenderTexture*, m_TAARTCount> m_TAARTs;

        struct ShaderIDs
        {
            static inline size_t g_DestTextureIndexID = PropertyToID(L"g_DestTextureIndex");
            static inline size_t g_SourceTextureIndex = PropertyToID(L"g_SourceTextureIndex");
            static inline size_t g_DestSize = PropertyToID(L"g_DestSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
        };
    };
}