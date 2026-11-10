#include "private\ShadingCommon.hlsl"
#include "private/SSSR/SSSRCommon.hlsli"

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_TargetSize;
    float g_RoughnessThreshold;
    uint g_SamplesPerQuad;

    UINT g_RayCounterBufferIndex;
    UINT g_RayListBufferIndex;
    UINT g_IntersectionOutputTexIndex;
    UINT g_IntersectionArgsBufferIndex;
    UINT g_TileClassifyDebugTexIndex;
    UINT g_TileClassifyDebugView;
    
}

#ifndef SSSR_TILE_CLASSIFY_GROUP_SIZE
#define SSSR_TILE_CLASSIFY_GROUP_SIZE 8
#endif

#ifndef SSSR_INTERSECT_ARGS_GROUP_SIZE
#define SSSR_INTERSECT_ARGS_GROUP_SIZE 1
#endif

#ifndef SSSR_INTERSECT_GROUP_SIZE
#define SSSR_INTERSECT_GROUP_SIZE 8
#endif

#define SSSR_TILE_CLASSIFY_THREAD_GROUP [numthreads(SSSR_TILE_CLASSIFY_GROUP_SIZE, SSSR_TILE_CLASSIFY_GROUP_SIZE, 1)]
#define SSSR_INTERSECT_ARGS_THREAD_GROUP [numthreads(SSSR_INTERSECT_ARGS_GROUP_SIZE, SSSR_INTERSECT_ARGS_GROUP_SIZE, 1)]
#define SSSR_INTERSECT_THREAD_COUNT (SSSR_INTERSECT_GROUP_SIZE * SSSR_INTERSECT_GROUP_SIZE)

#define SSSR_TILE_CLASSIFY_DEBUG_CLASSIFICATION 0
#define SSSR_TILE_CLASSIFY_DEBUG_COPY_FLAGS 1
// Sentinel from SSSRPass when TileClassify debug is off - skip UAV writes.
#define SSSR_TILE_CLASSIFY_DEBUG_DISABLED 0xFFFFFFFFu

float GetRoughness(UINT2 LoadPos)
{
    return LoadTexture2D(GBuffer1Index, LoadPos).b;
}

float GetSceneDepth(UINT2 LoadPos)
{
    return LoadTexture2D(OpaqueDepthIndex, LoadPos).r;
}

bool IsDepthMatch(float Linear01SceneDepth)
{
    return Linear01SceneDepth < 0.999f;
}

bool IsRoughnessMatch(float Roughness)
{
    return Roughness < g_RoughnessThreshold;
}

bool IsShootRay(UINT2 PixelPos)
{
    switch (g_SamplesPerQuad)
    {
        case 1:
            return (((PixelPos.x & 1) | (PixelPos.y & 1)) == 0);
        case 2:
            return (PixelPos.x & 1) == (PixelPos.y & 1);
        case 4:
            return true;
        default:
            return true;
    }
}

void IncrementRayCounter(UINT AddValue, out UINT SourceValue)
{
    RWStructuredBuffer<UINT> RayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    InterlockedAdd(RayCounterBuffer[0], AddValue, SourceValue);
}

void StoreRay(UINT RayIndex, UINT2 RayPos, bool bCopyHorizonal, bool bCopyVertical, bool bCopyDiagonal)
{
    RWStructuredBuffer<UINT> RayListBuffer = ResourceDescriptorHeap[g_RayListBufferIndex];
    UINT PackData = PackRayData(RayPos, bCopyHorizonal, bCopyVertical, bCopyDiagonal);
    RayListBuffer[RayIndex] = PackData;
}

void SampleEnvironment(UINT2 ReadPos, float Roughness)
{
    float3 dimensions = GetTexture2DDimensions(SkyboxTexIndex);

    const float MAX_REFLECTION_LOD = max(dimensions.z - 1.0f, 0.0f);
    float lod = Roughness * MAX_REFLECTION_LOD;

    float2 sampleUV = ((float2)ReadPos + 0.5f) * g_TargetSize.zw;
    float sampleDepth = LoadTexture2D(OpaqueDepthIndex, ReadPos).r;

    float3 normalWS = DecodeNormal(LoadTexture2D(GBuffer3Index, ReadPos));
    float3 positionWS = ComputeWorldSpacePosition(sampleUV, sampleDepth, viewProjMatrix_I);
    float3 viewDirWS = normalize(cameraPosWS - positionWS);

    float3 R = 2 * dot(viewDirWS, normalWS) * normalWS - viewDirWS;
    float4 specularIBL = SampleTextureCube_LOD(SkyboxTexIndex, R, ClampLinearSampler, lod);

    RWTexture2D<float4> IntersectionOutputTex = ResourceDescriptorHeap[g_IntersectionOutputTexIndex];
    // Alpha 0: this is an IBL fallback already present in deferred lighting.
    // Composite only adds intersection results with confidence > 0.
    IntersectionOutputTex[ReadPos] = float4(specularIBL.rgb, 0.0f);
}

void WriteTileClassifyDebug(UINT2 WritePos,
                            bool bValid,
                            bool bShootRay,
                            bool bSampleEnvironment,
                            bool bCopyHorizonal,
                            bool bCopyVertical,
                            bool bCopyDiagonal)
{
    RWTexture2D<float4> DebugTex = ResourceDescriptorHeap[g_TileClassifyDebugTexIndex];

    float4 color = float4(0.05, 0.05, 0.05, 1.0);

    if (g_TileClassifyDebugView == SSSR_TILE_CLASSIFY_DEBUG_COPY_FLAGS)
    {
        if (bShootRay)
        {
            color = float4(bCopyHorizonal ? 1.0 : 0.0,
                           bCopyVertical ? 1.0 : 0.0,
                           bCopyDiagonal ? 1.0 : 0.0,
                           1.0);
        }
        else if (bSampleEnvironment)
        {
            color = float4(0.15, 0.15, 0.35, 1.0);
        }
        else if (bValid)
        {
            color = float4(0.35, 0.35, 0.1, 1.0);
        }
    }
    else
    {
        // Classification: sky/invalid=dark, env=blue, shoot=red, copy-candidate=yellow
        if (bShootRay)
            color = float4(1.0, 0.2, 0.2, 1.0);
        else if (bValid)
            color = float4(1.0, 0.9, 0.2, 1.0);
        else if (bSampleEnvironment)
            color = float4(0.2, 0.45, 1.0, 1.0);
    }

    DebugTex[WritePos] = color;
}

void TileClassify(UINT2 ReadPos, UINT2 WritePos, UINT2 GroupThreadID)
{
    bool IsFirstThread = WaveIsFirstLane();

    float Roughness = GetRoughness(ReadPos);
    float SceneDepth = GetSceneDepth(ReadPos);
    float Linear01SceneDepth = Linear01Depth(SceneDepth, g_ZBufferParams);

    bool bOutWorkArea = SSSR_IsThreadOutWorkArea(ReadPos, g_TargetSize);
    bool bDepthMatch = IsDepthMatch(Linear01SceneDepth);
    bool bRoughnessMatch = IsRoughnessMatch(Roughness);

    bool bValid = !bOutWorkArea && bDepthMatch && bRoughnessMatch;
    bool bIsBase = IsShootRay(ReadPos);
    bool bShootRay = bValid && bIsBase;

    UINT SumShootRayInThreadGroup = WaveActiveCountBits(bShootRay);
    UINT CurrThreadIndex = WavePrefixCountBits(bShootRay);
    UINT BaseThreadIndex = 0;
    if (IsFirstThread)
    {
        IncrementRayCounter(SumShootRayInThreadGroup, BaseThreadIndex);
    }
    BaseThreadIndex = WaveReadLaneFirst(BaseThreadIndex);

    bool bNeedCopy = bValid && !bIsBase;
    bool bCopyHorizonal = g_SamplesPerQuad != 4 && bShootRay && WaveReadLaneAt(bNeedCopy, WaveGetLaneIndex() ^ 0b01);
    bool bCopyVertical = g_SamplesPerQuad == 1 && bShootRay && WaveReadLaneAt(bNeedCopy, WaveGetLaneIndex() ^ 0b10);
    bool bCopyDiagonal = g_SamplesPerQuad == 1 && bShootRay && WaveReadLaneAt(bNeedCopy, WaveGetLaneIndex() ^ 0b11);

    BRANCH
    if (bShootRay)
    {
        UINT RayIndex = BaseThreadIndex + CurrThreadIndex;
        StoreRay(RayIndex, ReadPos, bCopyHorizonal, bCopyVertical, bCopyDiagonal);
    }

    // Clear intersection output for pixels that are not falling back to env.
    RWTexture2D<float4> IntersectionOutputTex = ResourceDescriptorHeap[g_IntersectionOutputTexIndex];
    bool bSampleEnvironment = !bOutWorkArea && bDepthMatch && !bRoughnessMatch;
    BRANCH
    if (bSampleEnvironment)
    {
        SampleEnvironment(ReadPos, Roughness);
    }
    else if (!bOutWorkArea)
    {
        IntersectionOutputTex[ReadPos] = 0;
    }

    if (!bOutWorkArea && g_TileClassifyDebugView != SSSR_TILE_CLASSIFY_DEBUG_DISABLED)
    {
        WriteTileClassifyDebug(WritePos,
                               bValid,
                               bShootRay,
                               bSampleEnvironment,
                               bCopyHorizonal,
                               bCopyVertical,
                               bCopyDiagonal);
    }
}

[numthreads(1, 1, 1)]
void ClearRayCounter()
{
    RWStructuredBuffer<UINT> RayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    RayCounterBuffer[0] = 0;
    RayCounterBuffer[1] = 0;
}

SSSR_TILE_CLASSIFY_THREAD_GROUP
void DoTileClassify(UINT3 ID : SV_DispatchThreadID,
    UINT2 groupID : SV_GroupID,
    UINT groupIndex : SV_GroupIndex)
{
    UINT2 GroupThreadID = SSSR_RemapLane8x8(groupIndex);
    UINT2 DispathThreadID = groupID * SSSR_TILE_CLASSIFY_GROUP_SIZE + GroupThreadID;

    UINT2 ReadPos = DispathThreadID;
    UINT2 WritePos = DispathThreadID;

    TileClassify(ReadPos, WritePos, GroupThreadID);
}

SSSR_INTERSECT_ARGS_THREAD_GROUP
void DoIntersectArgs(UINT3 ID : SV_DispatchThreadID)
{
    BRANCH
    if (any(ID != 0))
    {
        return;
    }
    
    RWStructuredBuffer<UINT> IntersectionArgsBuffer = ResourceDescriptorHeap[g_IntersectionArgsBufferIndex];
    RWStructuredBuffer<UINT> RayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    
    UINT RayCount = RayCounterBuffer[0];
    
    IntersectionArgsBuffer[0] = (RayCount + (SSSR_INTERSECT_THREAD_COUNT - 1u)) / SSSR_INTERSECT_THREAD_COUNT;
    IntersectionArgsBuffer[1] = 1;
    IntersectionArgsBuffer[2] = 1;
    
    RayCounterBuffer[0] = 0;
    RayCounterBuffer[1] = RayCount;
}
