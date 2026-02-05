#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaCore
{
    class DX12TextureResource;
}

namespace ElysiaRenderer
{
    class GBufferPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static size_t GBuffer0ID;
            static size_t GBuffer1ID;
            static size_t GBuffer2ID;
            static size_t GBuffer3ID;
            static size_t GBuffer4ID;
            static size_t GBuffer5ID;
        };

        GBufferPass();
        virtual ~GBufferPass() override;

        //virtual void Setup(const RenderPassData& renderPassData) override;
        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
        struct ShaderPassIDs
        {
            static int GBufferPassID;
        };

        struct ShaderIDs
        {
            static size_t screenSize;
            static size_t viewMatrix;
            static size_t viewMatrix_I;
            static size_t projMatrix;
            static size_t projMatrix_I;
            static size_t viewProjMatrix;
            static size_t viewProjMatrix_I;
            static inline size_t pre_viewMatrix = PropertyToID(L"pre_viewMatrix");
            static inline size_t pre_viewMatrix_I = PropertyToID(L"pre_viewMatrix_I");
            static inline size_t pre_projMatrix = PropertyToID(L"pre_projMatrix");
            static inline size_t pre_projMatrix_I = PropertyToID(L"pre_projMatrix_I");
            static inline size_t pre_viewProjMatrix = PropertyToID(L"pre_viewProjMatrix");
            static inline size_t pre_viewProjMatrix_I = PropertyToID(L"pre_viewProjMatrix_I");
            static size_t worldMatrix;
            static size_t opacity;
            static size_t cutoff;
            static size_t baseColorTexIndex;
            static size_t normalTexIndex;
            static size_t metallicTexIndex;
            static size_t roughnessTexIndex;
            static size_t specularTexIndex;
            static size_t baseColorTint;
            static size_t ambientCubemapTint;
            static size_t normalIntensity;
            static size_t metallicIntensity;
            static size_t roughnessIntensity;
            static size_t ambientCubemapIntensity;
            static size_t GBuffer0Index;
            static size_t GBuffer1Index;
            static size_t GBuffer2Index;
            static size_t GBuffer3Index;
            static size_t GBuffer4Index;
            static size_t GBuffer5Index;
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
        std::vector<RenderTexture*> m_GBufferRTs{};
        static inline bool m_isFirstFrame = true;

        std::vector<DX12TextureResource*> GetGBuffers();
        void CreateRTs();
        void UpdateGBufferPassVariant(UINT passIndex);
        void DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex);
        void DrawGBufferPass(ElysiaEngine::FrameContext& context);
    };
}