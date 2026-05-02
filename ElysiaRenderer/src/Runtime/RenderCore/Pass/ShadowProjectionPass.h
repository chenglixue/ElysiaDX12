#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{


    class ShadowProjectionPass : public BasePass
    {
#define SHADOW_PROJECTION_PASS_LIST \
PASS(SHADOW_PROJECTION_PASS,                "public\\CS_ScreenSpaceShadow.hlsl",               true,  ScreenSpaceShadow)\
PASS(SHADOW_TAA_PASS,                       "public\\CS_ScreenSpaceShadow.hlsl",               true,  ShadowTAA)

    public:
        struct RenderTextureIDs
        {
            static inline size_t ShadowMaskRTID = PropertyToID(L"Shadow Mask RT");
            static inline size_t ThicknessInLightRTID = PropertyToID(L"Thickness In Light RT");
        };
        static inline int m_writeIndex = 0;
        static inline int m_readIndex = 0;
        const static UINT m_ShadowMaskRTCount = 2;
        static inline std::array<RenderTexture*, m_ShadowMaskRTCount> m_pShadowMaskRTs;

    public:
        ShadowProjectionPass();
        virtual ~ShadowProjectionPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            SHADOW_PROJECTION_PASS_LIST
#undef PASS
            SHADOW_PROJECTION_PASS_COUNT
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
            SHADOW_PROJECTION_PASS_LIST
#undef PASS
        };
#pragma endregion
        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_shadowMaskWidth;
        UINT m_shadowMaskHeight;
        UINT m_sobolWidth;
        UINT m_sobolHeight;
        static inline bool m_isFirstFrame = true;

        std::vector<Vector2> m_sobolSequences;
        RenderTexture* m_pThicknessTexInLight = nullptr;

        struct ShaderIDs
        {
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");

            static inline size_t g_ShadowMaskTexIndex = PropertyToID(L"g_ShadowMaskTexIndex");
            static inline size_t g_SobolTexIndex = PropertyToID(L"g_SobolTexIndex");
            static inline size_t g_HistoryTexIndex = PropertyToID(L"g_HistoryTexIndex");
            static inline size_t g_CurrTexIndex = PropertyToID(L"g_CurrTexIndex");

            static inline size_t g_ShadowMaskTexSize = PropertyToID(L"g_ShadowMaskTexSize");
            static inline size_t g_ScreenSize = PropertyToID(L"g_ScreenSize");

            static inline size_t g_SobolSequence = PropertyToID(L"g_SobolSequence");
            static inline size_t g_StaticBlendWeight = PropertyToID(L"g_StaticBlendWeight");
            static inline size_t g_DynamicBlendWeight = PropertyToID(L"g_DynamicBlendWeight");
            static inline size_t g_MaxBlendWeight = PropertyToID(L"g_MaxBlendWeight");
            static inline size_t g_RandomSeed = PropertyToID(L"g_RandomSeed");
        };
        void DoShadowMask();
        void DoTAA();
    };
}