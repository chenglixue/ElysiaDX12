#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;

    class OpaquePass : public BasePass
    {
#define OPAQUE_LIGHT_PASS_LIST \
PASS(DRAW_LIGHT_PASS,         "public\\Opaque.hlsl", false, PS)

    public:
        OpaquePass();
        virtual ~OpaquePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            OPAQUE_LIGHT_PASS_LIST
#undef PASS
            OPAQUE_LIGHT_PASS_COUNT
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
            OPAQUE_LIGHT_PASS_LIST
#undef PASS
        };
#pragma endregion
        struct ShaderIDs
        {
            static inline size_t g_DebugMode = PropertyToID(L"g_DebugMode");
            static inline size_t g_RenderSize = PropertyToID(L"g_RenderSize");

            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");

            static inline size_t g_AOIndex = PropertyToID(L"g_AOIndex");
            static inline size_t g_AmbientTint = PropertyToID(L"g_AmbientTint");
            static inline size_t g_AmbientIntensity = PropertyToID(L"g_AmbientIntensity");
            static inline size_t g_ShadowMaskTexIndex = PropertyToID(L"g_ShadowMaskTexIndex");

            static inline size_t g_PreIntegrateSSSLUTIndex = PropertyToID(L"g_PreIntegrateSSSLUTIndex");
            static inline size_t g_PreIntegrateSSSNDFLUTIndex = PropertyToID(L"g_PreIntegrateSSSNDFLUTIndex");
            static inline size_t g_CurveScale = PropertyToID(L"g_CurveScale");
            static inline size_t g_ScatterRadius = PropertyToID(L"g_ScatterRadius");

            static inline size_t g_SHCoefficientsBufferIndex = PropertyToID(L"g_SHCoefficientsBufferIndex");
        };

        UINT m_cameraWidth;
        UINT m_cameraHeight;
        TextureManager::Handle m_PreIntegrateSSSLUT;
        TextureManager::Handle m_PreIntegrateSSSNDFLUT;

        void UpdateLightingPassVariant(UINT passID);
        void DrawLightingPass(ElysiaEngine::FrameContext& context);
    };
}