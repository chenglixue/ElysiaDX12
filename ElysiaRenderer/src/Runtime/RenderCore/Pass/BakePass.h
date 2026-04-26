#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class BakePass : public BasePass
    {
    public:
#define BAKE_PASS_LIST \
        PASS(CS_PRE_INTEGRATE_SSS,      "public\\PreGen\\CS_PreIntegrateSSS.hlsl",   true,  PreIntegrateSSS)\
        PASS(CS_INTEGRATE_SSS_NDF,      "public\\PreGen\\CS_PreIntegrateSSS.hlsl",   true,  IntegrateSSSNDF)

        struct RenderTextureIDs
        {
            static inline size_t PreIntegrateSSSLUTID = PropertyToID(L"Pre Integrate SSS LUT");
            static inline size_t IntegrateSSSNDFLUTID = PropertyToID(L"Integrate SSS NDF LUT");
            static inline size_t PreIntegrateDiffuseID = PropertyToID(L"Pre Integrate Diffuse");
        };

        BakePass();
        virtual ~BakePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            BAKE_PASS_LIST
#undef PASS
            BAKE_PASS_COUNT
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
            BAKE_PASS_LIST
#undef PASS
        };
#pragma endregion

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        RenderTexture* m_pPreIntegrateSSSLUT = nullptr;
        RenderTexture* m_pNDFLUT = nullptr;
        RenderTexture* m_pPreIntegrateDiffuse = nullptr;

        struct ShaderIDs
        {
            static inline size_t g_PreIntegrateSSSLUTIndex = PropertyToID(L"g_PreIntegrateSSSLUTIndex");
            static inline size_t g_IntegrateSSSNDFLUTIndex = PropertyToID(L"g_IntegrateSSSNDFLUTIndex");

            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
        };

        void DoPreIntegrateSSSLUT();
        void DoIntegrateSSSNDFLUT();
    };
}