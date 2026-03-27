#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class SharpenPass : public BasePass
    {
#define SHARPEN_PASS_LIST \
PASS(CAS_PASS,          "public\\PostProcess\\Sharpen\\CS_CAS.hlsl",               true,  CAS)

    public:
        struct RenderTextureIDs
        {
            static inline size_t SharpenRTID = PropertyToID(L"Sharpen RT");
        };

    public:
        SharpenPass();
        virtual ~SharpenPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            SHARPEN_PASS_LIST
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
            SHARPEN_PASS_LIST
#undef PASS
        };
#pragma endregion
        UINT m_renderWidth;
        UINT m_renderHeight;

        struct ShaderIDs
        {
            static inline size_t g_SharpenTexSize = PropertyToID(L"g_SharpenTexSize");
            static inline size_t g_SharpenTexIndex = PropertyToID(L"g_SharpenTexIndex");
            static inline size_t g_SharpenIntensity = PropertyToID(L"g_SharpenIntensity");
        };
        void DoCAS();
    };
}