#pragma once
#include "BasePass.h"
#include "Programs/SBTHelper.h"
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

#define GI_PASS_LIST \
    PASS(RELOCATE_PROBES_PASS,          "public\\GI\\CS_DDGI.hlsl",               true,  RelocateProbes)\
    PASS(CLEAR_PROBE_OFFSET_PASS,       "public\\GI\\CS_DDGI.hlsl",               true,  ClearProbeOffsetBuffer)\
    PASS(PROBE_BLENDING_PASS,           "public\\GI\\CS_DDGI.hlsl",               true,  ProbeBlending)\
    PASS(UPDATE_PROBE_STATES,           "public\\GI\\CS_DDGI.hlsl",               true,  UpdateProbeStates)\
    PASS(RESET_PROBE_STATES,            "public\\GI\\CS_DDGI.hlsl",               true,  ResetProbeStates)\
    PASS(RESET_PROBE_OFFSET_INDEX,      "public\\GI\\CS_RrobeOffset.hlsl",        true,  ResetProbeOffsetIndex)

    class GIPass : public BasePass
    {
    public:
        struct RenderTextureIDs
        {
            static inline size_t GIRTID = PropertyToID(L"GI RT");
            static inline size_t DistanceRTID = PropertyToID(L"Distance RT");
            static inline size_t IrradianceRTID = PropertyToID(L"Irradiance RT");
            static inline size_t ProbeOffsetIndexRTID = PropertyToID(L"Probe Offset Index RT");
        };
        struct ShaderIDs
        {
            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_IrradianceTexIndex = PropertyToID(L"g_IrradianceTexIndex");
            static inline size_t g_DistanceTexIndex = PropertyToID(L"g_DistanceTexIndex");
            static inline size_t g_IrradianceTexSize = PropertyToID(L"g_IrradianceTexSize");
            static inline size_t g_DistanceTexSize = PropertyToID(L"g_DistanceTexSize");
            static inline size_t g_RayDataBufferIndex = PropertyToID(L"g_RayDataBufferIndex");

            static inline size_t g_GridSpacing = PropertyToID(L"g_GridSpacing");
            static inline size_t g_GridOrigin = PropertyToID(L"g_GridOrigin");
            static inline size_t g_GridDimensions = PropertyToID(L"g_GridDimensions");
            static inline size_t g_ProbeRadius = PropertyToID(L"g_ProbeRadius");
            static inline size_t g_RandomRotation = PropertyToID(L"g_RandomRotation");
            static inline size_t g_ProbeOffsetsIndex = PropertyToID(L"g_ProbeOffsetsIndex");
            static inline size_t g_ProbeStatesIndex = PropertyToID(L"g_ProbeStatesIndex");
            static inline size_t g_RelocationLUTIndex = PropertyToID(L"g_RelocationLUTIndex");
            static inline size_t g_StaticAABBIndex = PropertyToID(L"g_StaticAABBIndex");
            static inline size_t g_ProbeNormalBias = PropertyToID(L"g_ProbeNormalBias");
            static inline size_t g_ProbeViewBias = PropertyToID(L"g_ProbeViewBias");
            static inline size_t g_DDGIBlendWeight = PropertyToID(L"g_DDGIBlendWeight");
            static inline size_t g_DDGIEncodingGamma = PropertyToID(L"g_DDGIEncodingGamma");
            static inline size_t g_ProbeIrradianceThreshold = PropertyToID(L"g_ProbeIrradianceThreshold");
            static inline size_t g_ProbeBrightnessThreshold = PropertyToID(L"g_ProbeBrightnessThreshold");
            static inline size_t g_StaticAABBCount = PropertyToID(L"g_StaticAABBCount");
            static inline size_t g_ProbeOffsetIndexTexIndex = PropertyToID(L"g_ProbeOffsetIndexTexIndex");
            static inline size_t g_ProbeRelocationLUTBufferIndex = PropertyToID(L"g_ProbeRelocationLUTBufferIndex");
            static inline size_t g_DDGI_Probe_Num_Texels = PropertyToID(L"g_DDGI_Probe_Num_Texels");
            static inline size_t g_IsBlendIrradiance = PropertyToID(L"g_IsBlendIrradiance");
        };

        static inline D3D12_VERTEX_BUFFER_VIEW m_vertexView;
        static inline D3D12_INDEX_BUFFER_VIEW m_indexView;
        static inline Vector3 m_gridSpacing;
        static inline Vector3 m_gridOrigin;
        static inline BufferHandle m_vertexBuffer;
        static inline BufferHandle m_indexBuffer;
        static inline BufferHandle m_pRayDataBuffer;
        static inline BufferHandle m_pInstanceDataBuffer;
        // static inline BufferHandle m_pProbeOffsetBuffer;
        static inline BufferHandle m_pProbeStateBuffer;
        static inline BufferHandle m_pProbeRelocationLUTBuffer;
        static inline RenderTexture* m_pProbeOffsetIndexRT = nullptr;
        static inline RenderTexture* m_pIrradianceRT = nullptr;
        static inline RenderTexture* m_pDistanceRT = nullptr;
        static inline constexpr UINT NumVertices = 12;
        static inline constexpr UINT NumIndices = 60;
        static inline constexpr UINT Probe_Count = 10648;
        static inline const UINT3 Grid_Dimensions = UINT3(22, 22, 22);
        static inline constexpr UINT Rays_Per_Probe = 256;
        static inline Vector4 m_RandomRotation;

    public:
        GIPass();
        virtual ~GIPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
#pragma region PASS
        enum PassID
        {
#define PASS(id, file, isCS, entry) id,
            GI_PASS_LIST
#undef PASS
            GI_PASS_COUNT
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
            GI_PASS_LIST
#undef PASS
        };

#pragma endregion

        static constexpr float k_GoldenAngle = 2.39996322972865332f;
        static constexpr auto Max_RenderItem_Count = 10240;

        UINT m_frameIndex;
        UINT m_halfWidth;
        UINT m_halfHeight;
        UINT m_quarterWidth;
        UINT m_quarterHeight;

        RenderTexture* m_pGIRT = nullptr;

        /// --------------------------------------------
        /// DXR
        /// --------------------------------------------
        struct alignas(16) RayData
        {
            Vector3 Radiance;
            float Distance;
        };
        struct alignas(16) InstanceData
        {
            UINT BaseColorTexIndex;
            UINT NormalTexIndex;
            UINT MetallicTexIndex;
            UINT RoughnessTexIndex;

            UINT VertexOffset;
            UINT IndexOffset;
            UINT VertexBufferIndex;
            UINT IndexBufferIndex;
        };
        struct alignas(16) AABBData
        {
            Vector3 Min;
            float pad0;
            Vector3 Max;
            float pad1;
        };
        CComPtr<ID3D12Device5> m_pDevice5;
        SBTHelper m_stbHelper;
        CComPtr<IDxcBlob> m_DXRBlob;
        CComPtr<ID3D12RootSignature> m_pGlobalRootSig;
        CComPtr<ID3D12StateObject> m_pRTPSO;
        mutable std::vector<std::wstring> m_tempStrings;
        BufferHandle m_pTLASBuffer;
        BufferHandle m_pTLASScratchBuffer;
        BufferHandle m_pTLASUploadBuffer;
        BufferHandle m_pStaticAABBDataBuffer;
        std::vector<InstanceData> m_instanceDatas;
        std::vector<AABBData> m_AABBDatas;
        std::vector<UINT> m_probeStates = std::vector<UINT>(Probe_Count, 1);


#pragma region Vertices
        // 常量 X = 0.525731f, Z = 0.850651f 来自黄金分割比例，用于构建单位球体
        const
        float X = 0.525731f;
        const float Z = 0.850651f;
        const float N = 0.0f;
        const Vector3 m_probeVertices[NumVertices] =
        {
            Vector3(-X, N, Z), Vector3(X, N, Z), Vector3(-X, N, -Z), Vector3(X, N, -Z),
            Vector3(N, Z, X), Vector3(N, Z, -X), Vector3(N, -Z, X), Vector3(N, -Z, -X),
            Vector3(Z, X, N), Vector3(-Z, X, N), Vector3(Z, -X, N), Vector3(-Z, -X, N)
        };

#pragma endregion

#pragma region Indices
        // 20个面，60个索引 (CCW - 逆时针缠绕顺序)
        static constexpr INDEX_FORMAT m_probeIndices[NumIndices] =
        {
            0, 4, 1, 0, 9, 4, 9, 5, 4, 4, 5, 8, 4, 8, 1,
            8, 10, 1, 8, 3, 10, 5, 3, 8, 5, 2, 3, 2, 7, 3,
            7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6,
            6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5, 7, 2, 11
        };
#pragma endregion

        void InitProbeOffsetIndex();
        void ResetProbeStates();
        void UpdateProbeStates();
        void GenerateRay();
        void ClearProbeOffset();
        void RelocateProbes();
        void ProbeBlend();

        CComPtr<IDxcBlob> CompileRaytracingLibrary(const std::wstring& fileName);
        void CreateRaytracingPipeline(ID3D12RootSignature* pRootSignature,
                                      const std::vector<std::unique_ptr<Entity>>& entities);
        void CreateDXRRootSignature(ID3D12Device* pDevice);
        void GenerateTLAS(const std::vector<std::unique_ptr<Entity>>& entityies);
        std::vector<D3D12_SAMPLER_DESC> GenerateSampler();

        void ComputeRandomRotation();
    };

    static Vector3 GetFibonacciDir(int i);
    static float GetDistScale(int i);
    static std::vector<Vector4> GenerateRelocationLUT(Vector3 probeSpacing);
}