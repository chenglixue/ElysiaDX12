#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/AOUtility.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace CAULDRON_DX12;
    using namespace ElysiaHelper;

#define AO_PASS_LIST \
PASS(Deinterleaved_Depth_PASS,          "public\\PostProcess\\AO\\CS_LayeredAO.hlsl",               true, DeinterleaveMain)\
PASS(AO_HIZ_PASS,                       "public\\PostProcess\\AO\\CS_AOHIZNormal.hlsl",             true, AOHIZNormal)\
PASS(Deinterleaved_AO_PASS,             "public\\PostProcess\\AO\\CS_LayeredAO.hlsl",               true, CalcBaseAO)\
PASS(Generate_AO_Importance_PASS,       "public\\PostProcess\\AO\\CS_GenerateAOImportance.hlsl",    true, GenerateAOImportance)\
PASS(Post_AO_Importance_A,              "public\\PostProcess\\AO\\CS_GenerateAOImportance.hlsl",    true, PostAOImportanceA)\
PASS(Post_AO_Importance_B,              "public\\PostProcess\\AO\\CS_GenerateAOImportance.hlsl",    true, PostAOImportanceB)\
PASS(Calc_AO_PASS,                      "public\\PostProcess\\AO\\CS_LayeredAO.hlsl",               true, LayeredHBAOMain)\
PASS(Deinterleaved_Blur_PASS,           "public\\PostProcess\\AO\\CS_AOEdgeSensitiveBlur.hlsl",     true, AOEdgeSensitiveBlur)\
PASS(AO_Reinterleave_PASS,              "public\\PostProcess\\AO\\CS_LayeredAO.hlsl",               true, ReinterleaveMain)\
PASS(AO_TAA_PASS,                       "public\\PostProcess\\CS_AOTAA.hlsl",                       true, TAA)

    class AOPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t HIZRTID = PropertyToID(L"AO HIZ RT");
            static inline size_t AOImportanceID = PropertyToID(L"AO Importance RT");
            static inline size_t AORTID = PropertyToID(L"AO RT");
            static inline size_t TAA0RTID = PropertyToID(L"AO TAA0 RT");
            static inline size_t TAA1RTID = PropertyToID(L"AO TAA1 RT");
            static inline size_t AOBlurHorizonRTID = PropertyToID(L"AO Blur Horizon RT");
            static inline size_t AOBlurVerticalRTID = PropertyToID(L"AO Blur Vertical RT");
        };

    public:
        AOPass();
        virtual ~AOPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
        UINT m_HIZMipmapCount;
        static inline bool m_isFirstFrame = true;
        int m_currHistoryIndex = 0;
        static constexpr UINT MAX_BLUR_RADIUS = 10;
        static constexpr UINT DEINTERLEAVED_DEPTH_COUNT = 4;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_halfWidth;
        UINT m_halfHeight;
        UINT m_quarterWidth;
        UINT m_quarterHeight;
        UINT m_DeinterleavedDepthWidth;
        UINT m_DeinterleavedDepthHeight;
        UINT m_DeinterleavedAOWidth;
        UINT m_DeinterleavedAOHeight;
        UINT m_DeinterleavedBlurWidth;
        UINT m_DeinterleavedBlurHeight;
        UINT m_ImportanceWidth;
        UINT m_ImportanceHeight;

        RenderTexture* m_pAORT = nullptr;
        RenderTexture* m_pImportanceRT = nullptr;
        RenderTexture* m_pTAA0RT = nullptr;
        RenderTexture* m_pTAA1RT = nullptr;
        std::vector<RenderTexture*> m_DeinterleavedDepthRTs;
        std::vector<RenderTexture*> m_DeinterleavedAORTs;
        std::vector<UINT> m_DeinterleavedDepthIndices;
        std::vector<UINT> m_DeinterleavedAOIndices;
        TextureManager::Handle m_blueNoise;

        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            AO_PASS_LIST
#undef PASS
            AO_PASS_COUNT
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
            AO_PASS_LIST
#undef PASS
        };

        struct ShaderIDs
        {
            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_FullScreenSize = PropertyToID(L"g_FullScreenSize");
            static inline size_t g_DeinterleavedAOSize = PropertyToID(L"g_DeinterleavedAOSize");
            static inline size_t g_ImportanceBufferSize = PropertyToID(L"g_ImportanceBufferSize");

            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_TargetTexIndices = PropertyToID(L"g_TargetTexIndices");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_HistoryTex = PropertyToID(L"g_HistoryTex");
            static inline size_t g_ReinterleaveAOTexIndex = PropertyToID(
                L"g_ReinterleaveAOTexIndex");
            static inline size_t g_SourceTexIndices = PropertyToID(L"g_SourceTexIndices");
            static inline size_t g_DeinterleaveDepthTexIndices = PropertyToID(
                L"g_DeinterleaveDepthTexIndices");
            static inline size_t g_DeinterleaveAOTexIndices = PropertyToID(
                L"g_DeinterleaveAOTexIndices");
            static inline size_t g_DeinterleaveBlurTexIndices = PropertyToID(
                L"g_DeinterleaveBlurTexIndices");
            static inline size_t g_BlurTexIndex = PropertyToID(
                L"g_BlurTexIndex");

            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t g_ProjectScale = PropertyToID(L"g_ProjectScale");
            static inline size_t g_NDCToViewMul = PropertyToID(L"g_NDCToViewMul");
            static inline size_t g_NDCToViewAdd = PropertyToID(L"g_NDCToViewAdd");
            static inline size_t g_DepthUnpackConsts = PropertyToID(L"g_DepthUnpackConsts");

            static inline size_t pre_viewMatrix = PropertyToID(L"pre_viewMatrix");
            static inline size_t pre_viewMatrix_I = PropertyToID(L"pre_viewMatrix_I");
            static inline size_t pre_projMatrix = PropertyToID(L"pre_projMatrix");
            static inline size_t pre_projMatrix_I = PropertyToID(L"pre_projMatrix_I");
            static inline size_t pre_viewProjMatrix = PropertyToID(L"pre_viewProjMatrix");
            static inline size_t pre_viewProjMatrix_I = PropertyToID(L"pre_viewProjMatrix_I");

            static inline size_t g_AOSampleKernelArray = PropertyToID(L"g_AOSampleKernelArray");
            static inline size_t g_AOSampleCount = PropertyToID(L"g_AOSampleCount");
            static inline size_t g_AOSampleStepCount = PropertyToID(L"g_AOSampleStepCount");
            static inline size_t g_AORadius = PropertyToID(L"g_AORadius");
            static inline size_t g_AOFadeRadius = PropertyToID(L"g_AOFadeRadius");
            static inline size_t g_AOFadeDistance = PropertyToID(L"g_AOFadeDistance");
            static inline size_t g_AOBias = PropertyToID(L"g_AOBias");
            static inline size_t g_AOIntensityMul = PropertyToID(L"g_AOIntensityMul");
            static inline size_t g_AOIntensityPow = PropertyToID(L"g_AOIntensityPow");
            static inline size_t g_BlendWeight = PropertyToID(L"g_BlendWeight");

            static inline size_t g_AOIndex = PropertyToID(L"g_AOIndex");
            static inline size_t g_RandStepTexIndex = PropertyToID(L"g_RandStepTexIndex");
            static inline size_t g_noiseScale = PropertyToID(L"g_noiseScale");

            static inline size_t g_HIZMaxMipmap = PropertyToID(L"g_HIZMaxMipmap");
            static inline size_t g_HIZMipmap = PropertyToID(L"g_HIZMipmap");
            static inline size_t g_HIZTextureIndex = PropertyToID(L"g_HIZTextureIndex");

            static inline size_t g_BlurDir = PropertyToID(L"g_BlurDir");
            static inline size_t g_Sharpness = PropertyToID(L"g_Sharpness");
            static inline size_t g_Sharpness_Inv = PropertyToID(L"g_Sharpness_Inv");
            static inline size_t g_BlurRadius = PropertyToID(L"g_BlurRadius");
            static inline size_t g_Weights = PropertyToID(L"g_Weights");

            static inline size_t g_bImportance = PropertyToID(L"g_bImportance");
            static inline size_t g_AOImportanceTexIndex = PropertyToID(L"g_AOImportanceTexIndex");

            static inline size_t g_bDebugImportance = PropertyToID(L"g_bDebugImportance");
            static inline size_t g_bDebugHIZMipmap = PropertyToID(L"g_bDebugHIZMipmap");
            static inline size_t g_IsBlur = PropertyToID(L"g_IsBlur");
        };
        struct TAAData
        {
            static inline Matrix Pre_View_M;
            static inline Matrix Pre_Proj_M;
            static inline Matrix Pre_ViewProj_M;
            static inline Matrix Pre_View_I_M;
            static inline Matrix Pre_Proj_I_M;
            static inline Matrix Pre_ViewProj_I_M;
        };

        std::vector<Vector4> m_kernels;
        std::vector<float> m_blurWeights;

        void DoHIZ();
        void DoDeinterleaveDepth();
        void DoDeinterleaveBaseAO();
        void DoImportance();
        void DoDeinterleaveCalcAO();
        void DoBilateralBlur();
        void DoReinterleave();

        void DoTAA();

        std::vector<Vector4> GenerateSSAOSampleKernel();
        std::vector<Vector4> GenerateHBAOSampleKernel();
        std::vector<float> GenerateBlurWeights(UINT blurRadius);
    };
}