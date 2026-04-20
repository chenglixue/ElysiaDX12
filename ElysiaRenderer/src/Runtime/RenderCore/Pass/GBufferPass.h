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
    PASS(DRAW_GBUFFER_PASS,         "public\\GBuffer.hlsl",             false, PS)\
    PASS(CS_GBuffer_COPY_DEPTH,     "public\\CS_GBufferHIZ.hlsl",       true,  GBuffer_Copy_Depth)\
    PASS(CS_GBuffer_HIZ,            "public\\CS_GBufferHIZ.hlsl",       true,  GBuffer_HIZ)\
    PASS(CS_CLEAR_COUNTER_BUFFER,   "public\\CS_GBufferCulling.hlsl",   true,  ClearCounterBuffer)\
    PASS(CS_GBUFFER_CULLING_PASS,   "public\\CS_GBufferCulling.hlsl",   true,  Gbuffer_Culling)\
    PASS(CS_PRE_INTEGRATE_SSS,      "public\\PreGen\\CS_PreIntegrateSSS.hlsl",   true,  PreIntegrateSSS)

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
            static inline size_t GBufferHIZID = PropertyToID(L"GBuffer HIZ RT");
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
        static inline Vector2 m_currJitterUV, m_preJitterUV;
        static inline BufferHandle m_pVisbibleCounterReadBackBuffer;
        static inline int m_renderCount;

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
        int m_HIZMipmapCount;

        struct ShaderIDs
        {
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t jitterProjMatrix = PropertyToID(L"jitterProjMatrix");
            static inline size_t jitterProjMatrix_I = PropertyToID(L"jitterProjMatrix_I");
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
            static inline size_t g_AmbientIntensity = PropertyToID(L"g_AmbientIntensity");
            static inline size_t GBuffer0Index = PropertyToID(L"GBuffer_0");
            static inline size_t GBuffer1Index = PropertyToID(L"GBuffer_1");
            static inline size_t GBuffer2Index = PropertyToID(L"GBuffer_2");
            static inline size_t GBuffer3Index = PropertyToID(L"GBuffer_3");
            static inline size_t GBuffer4Index = PropertyToID(L"GBuffer_4");
            static inline size_t GBuffer5Index = PropertyToID(L"GBuffer_5");
            static inline size_t g_AmbientTint = PropertyToID(L"g_AmbientTint");

            static inline size_t g_AABBInstanceDatasIndex = PropertyToID(L"g_AABBInstanceDatasIndex");
            static inline size_t g_VisbibleCounterBufferIndex = PropertyToID(L"g_VisbibleCounterBufferIndex");
            static inline size_t g_VisbibleIndexBufferIndex = PropertyToID(L"g_VisbibleIndexBufferIndex");
            static inline size_t g_FrustumPlanes = PropertyToID(L"g_FrustumPlanes");
            static inline size_t g_TotalObjectCount = PropertyToID(L"g_TotalObjectCount");
            static inline size_t g_GBufferHIZSourceTexIndex = PropertyToID(L"g_GBufferHIZSourceTexIndex");
            static inline size_t g_GBufferHIZTargetTexIndex = PropertyToID(L"g_GBufferHIZTargetTexIndex");
            static inline size_t g_PreIntegrateSSSLUT = PropertyToID(L"g_PreIntegrateSSSLUT");

            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_HIZTexIndex = PropertyToID(L"g_HIZTexIndex");
            static inline size_t g_HIZTexSize = PropertyToID(L"g_HIZTexSize");
            static inline size_t g_EnableHIZ = PropertyToID(L"g_EnableHIZ");
            static inline size_t g_FrustumMaxPoint = PropertyToID(L"g_FrustumMaxPoint");
            static inline size_t g_FrustumMinPoint = PropertyToID(L"g_FrustumMinPoint");
            static inline size_t g_HIZMipmapCount = PropertyToID(L"g_HIZMipmapCount");
            static inline size_t g_InputViewportMaxBound = PropertyToID(L"g_InputViewportMaxBound");

        };
        struct MeshData
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

            Vector4 baseColorTint;
            Vector4 emissionColorTint;

            float roughnessIntensity;
            float normalIntensity;
            UINT emissionColorIndex;
            float specular;

            int shadingModelID;
            Vector3 padd;
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
        struct AABBLoader
        {
            struct alignas(16) AABBInstanceData
            {
                Vector3 Min;
                float pad0;
                Vector3 Max;
                float pad1;
            };
            std::vector<AABBInstanceData> m_instanceCpuData;
            BufferHandle instanceDataBuffer;

            void AddAABB(const Vector3& min, const Vector3& max);
            void Clear();
            void Bind(Material* pMaterail);

        };

        UINT m_displayWidth;
        UINT m_displayHeight;
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        UINT m_HIZWidth;
        UINT m_HIZHeight;
        Matrix m_currMatrixP;
        Matrix m_currMatrixVP;
        Matrix m_currMatrixVP_I;
        Matrix m_jitterMatrixProj;

        std::vector<RenderTexture*> m_GBufferRTs{};
        std::vector<DX12BufferUpload*> m_uploads;
        std::vector<MeshData> m_meshDatas;
        std::vector<IndirectCommand> m_indirectCommands;
        BufferHandle m_pMeshDataBuffer;
        BufferHandle m_pIndirectDataBuffer;
        BufferHandle m_pVisbibleCounterBuffer;
        BufferHandle m_pVisbibleIndexBuffer;
        RenderTexture* m_pHIZTex;
        static inline bool m_isFirstFrame = true;
        CComPtr<ID3D12CommandSignature> m_pCommandSignature;

        std::vector<RenderItem> m_cullRenderList;
        AABBLoader m_aabbLoader;

        std::vector<DX12TextureResource*> GetGBuffers();
        void CreateRTs();

        void UpdateCSVariant(UINT passIndex);
        void UpdateGBufferPassVariant(UINT passIndex);

        void UpdateTAAMatrices();
        void UploadMeshData(const std::vector<RenderItem>& renderItems);

        void CopyDepth();
        void DoHIZ();
        void DoPreIntegrateSSSLUT();

        void ClearCounterBuffer();
        // 从 ViewProjection 矩阵提取 6 个平面
        static std::vector<Vector4> ExtractFrustumPlanes(const Matrix& viewProj);
        void DoCulling(const std::vector<RenderItem>& renderItems);

        void DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex);
        void DrawGBufferPass(ElysiaEngine::FrameContext& context);

        void ReadGPUCounter();
    };
}