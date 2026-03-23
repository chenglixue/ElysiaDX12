#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
#define TAA_PASS_LIST \
PASS(TAA_PASS,          "public\\PostProcess\\TAA\\CS_TAA.hlsl",               true,  TAA)\
PASS(COPY_PASS,          "public\\PostProcess\\TAA\\CS_TAA.hlsl",               true,  CopyRT)
    class TAAPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t TAARTID = PropertyToID(L"TAA RT");
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
        UINT m_TAAWidth;
        UINT m_TAAHeight;

        static inline bool m_isFirstFrame = true;
        int m_writeIndex = 0;
        int m_readIndex = 0;
        const static UINT m_TAARTCount = 2;
        std::array<RenderTexture*, m_TAARTCount> m_TAARTs;

        struct ShaderIDs
        {
            static inline size_t g_HistoryTexIndex = PropertyToID(L"g_HistoryTexIndex");
            static inline size_t g_CurrTexIndex = PropertyToID(L"g_CurrTexIndex");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_DestTexIndex = PropertyToID(L"g_DestTexIndex");

            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t pre_viewProjMatrix = PropertyToID(L"pre_viewProjMatrix");
            static inline size_t g_ProjMatrix_I = PropertyToID(L"g_ProjMatrix_I");
            static inline size_t g_Jitter = PropertyToID(L"g_Jitter");
            static inline size_t g_HistoryJitter = PropertyToID(L"g_HistoryJitter");
            static inline size_t g_TAATexSize = PropertyToID(L"g_TAATexSize");
            static inline size_t g_StaticBlendWeight = PropertyToID(L"g_StaticBlendWeight");
            static inline size_t g_DynamicBlendWeight = PropertyToID(L"g_DynamicBlendWeight");
            static inline size_t g_MaxBlendWeight = PropertyToID(L"g_MaxBlendWeight");
        };

        void DoTAA();
        void DoFirstFrameCopyRT(RenderTexture* pSourceRT, RenderTexture* pDestRT);
        void DoCopyTAA2CameraColor();
    };
}