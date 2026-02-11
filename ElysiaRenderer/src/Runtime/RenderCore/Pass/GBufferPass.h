#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaCore
{
    class DX12TextureResource;
}

namespace ElysiaRenderer
{
#define GBUFFER_PASS_LIST \
PASS(DRAW_GBUFFER_PASS,         "public\\GBuffer.hlsl", false, PS)

    class GBufferPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t GBuffer0ID = PropertyToID(L"GBuffer0");
            static inline size_t GBuffer1ID = PropertyToID(L"GBuffer1");
            static inline size_t GBuffer2ID = PropertyToID(L"GBuffer2");
            static inline size_t GBuffer3ID = PropertyToID(L"GBuffer3");
            static inline size_t GBuffer4ID = PropertyToID(L"GBuffer4");
            static inline size_t GBuffer5ID = PropertyToID(L"GBuffer5");
        };

        GBufferPass();
        virtual ~GBufferPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
#pragma region Pass
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            GBUFFER_PASS_LIST
#undef PASS
            GBUFFER_PASS_COUNT
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
            GBUFFER_PASS_LIST
#undef PASS
        };
#pragma endregion
        static constexpr auto Max_RenderItem_Count = 1024;

        struct ShaderIDs
        {
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t pre_viewMatrix = PropertyToID(L"pre_viewMatrix");
            static inline size_t pre_viewMatrix_I = PropertyToID(L"pre_viewMatrix_I");
            static inline size_t pre_projMatrix = PropertyToID(L"pre_projMatrix");
            static inline size_t pre_projMatrix_I = PropertyToID(L"pre_projMatrix_I");
            static inline size_t pre_viewProjMatrix = PropertyToID(L"pre_viewProjMatrix");
            static inline size_t pre_viewProjMatrix_I = PropertyToID(L"pre_viewProjMatrix_I");
            static inline size_t worldMatrix = PropertyToID(L"worldMatrix");
            static inline size_t opacity = PropertyToID(L"opacity");
            static inline size_t cutoff = PropertyToID(L"cutoff");
            static inline size_t baseColorTexIndex = PropertyToID(L"baseColorTexIndex");
            static inline size_t normalTexIndex = PropertyToID(L"normalTexIndex");
            static inline size_t metallicTexIndex = PropertyToID(L"metallicTexIndex");
            static inline size_t roughnessTexIndex = PropertyToID(L"roughnessTexIndex");
            static inline size_t specularTexIndex = PropertyToID(L"specularTexIndex");
            static inline size_t baseColorTint = PropertyToID(L"baseColorTint");
            static inline size_t ambientCubemapTint = PropertyToID(L"ambientCubemapTint");
            static inline size_t normalIntensity = PropertyToID(L"normalIntensity");
            static inline size_t metallicIntensity = PropertyToID(L"metallicIntensity");
            static inline size_t roughnessIntensity = PropertyToID(L"roughnessIntensity");
            static inline size_t ambientCubemapIntensity = PropertyToID(L"ambientCubemapIntensity");
            static inline size_t GBuffer0Index = PropertyToID(L"GBuffer_0");
            static inline size_t GBuffer1Index = PropertyToID(L"GBuffer_1");
            static inline size_t GBuffer2Index = PropertyToID(L"GBuffer_2");
            static inline size_t GBuffer3Index = PropertyToID(L"GBuffer_3");
            static inline size_t GBuffer4Index = PropertyToID(L"GBuffer_4");
            static inline size_t GBuffer5Index = PropertyToID(L"GBuffer_5");
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
        struct alignas(16) MeshData
        {
            Matrix world_M;

            float opacity;
            float cutoff;
            UINT baseColorTexIndex;
            UINT normalTexIndex;

            UINT metallicTexIndex;
            UINT roughnessTexIndex;
            UINT specularTexIndex;
            float metallicIntensity;

            Vector3 baseColorTint;
            float roughnessIntensity;

            float normalIntensity;
            UINT vertexOffset;
            UINT indexOffset;
            UINT pad;
        };
        struct alignas(16) IndirectCommand
        {
            struct
            {
                UINT meshDataBufferIndex;
                UINT meshDataIndex;
            } pushConstants;

            D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
        };

        std::vector<RenderTexture*> m_GBufferRTs{};
        std::vector<DX12BufferUpload*> m_uploads;
        std::vector<MeshData> m_meshDatas;
        std::vector<IndirectCommand> m_indirectCommands;
        BufferHandle m_pMeshDataBuffer;
        BufferHandle m_pIndirectDataBuffer;
        static inline bool m_isFirstFrame = true;
        CComPtr<ID3D12CommandSignature> m_pCommandSignature;

        std::vector<DX12TextureResource*> GetGBuffers();
        void CreateRTs();
        void UpdateGBufferPassVariant(UINT passIndex);
        void DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex);
        void DrawGBufferPass(ElysiaEngine::FrameContext& context);
        void UploadMeshData(const std::vector<RenderItem>& renderItems);
    };
}