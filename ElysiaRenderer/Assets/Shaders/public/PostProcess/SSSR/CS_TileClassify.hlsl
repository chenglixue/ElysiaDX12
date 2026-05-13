#include "private\ShadingCommon.hlsl"
#include "private/SSSR/SSSRCommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_DestSize;
    float g_RoughnessThreshold;
    uint g_SamplesPerQuad;

    UINT g_RayCounterBufferIndex;
    UINT g_RayListBufferIndex;
    UINT g_IntersectionOutputTexIndex;
    UINT g_IntersectionArgsBufferIndex;
}

groupshared uint g_TileCount;

float GetRoughness(UINT2 readPos);
float GetDepth(UINT2 readPos);
void SaveIntersectionOutput(UINT2 writePos, float4 saveValue);

bool IsReflectiveSurface(UINT2 readPos);
bool IsGlossyReflection(UINT2 readPos);
bool IsShootRay(UINT2 readPos);
void IncrementRayCounter(UINT waveRayCount, out UINT baseRayIndex);
void StoreRay(UINT rayIndex, UINT2 rayCoord, bool copyHorizontal, bool copyVertical, bool copyDiagonal);
float4 CalcSpecular(UINT2 readPos, float Roughness);

void ClearRayCounter();
void DoTileClassify(UINT2 readPos, UINT2 groupThreadID, float roughness);
void CalcIntersectArgs();

[numthreads(1, 1, 1)]
void ClearRayCounterBuffer()
{
    ClearRayCounter();
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TileClassify(uint2 groupID : SV_GroupID,
                  uint groupIndex : SV_GroupIndex)
{
    // 0-64 -> 
    UINT2 groupThreadID = SSSR_RemapLane8x8(groupIndex);
    uint2 globalID = groupID * 8 + groupThreadID;

    UINT readPos = globalID;

    float roughness = GetRoughness(globalID);

    DoTileClassify(readPos, groupThreadID, roughness);
}

[numthreads(1, 1, 1)]
void DoIntersectArgs()
{
    CalcIntersectArgs();
}

float GetRoughness(UINT2 readPos)
{
    return LoadTexture2D(GBuffer1Index, readPos).b;
}

float GetDepth(UINT2 readPos)
{
    return LoadTexture2D(OpaqueDepthIndex, readPos);
}

void SaveIntersectionOutput(UINT2 writePos, float4 saveValue)
{
    RWTexture2D<float4> rayCounterBuffer = ResourceDescriptorHeap[g_IntersectionOutputTexIndex];
    rayCounterBuffer[writePos] = saveValue;
}

bool IsReflectiveSurface(UINT2 readPos)
{
    const float farPlaneZ = 1.f;
    return GetDepth(readPos) < farPlaneZ;
}

bool IsGlossyReflection(float roughness)
{
    return roughness < g_RoughnessThreshold;
}

bool IsShootRay(UINT2 readPos)
{
    switch (g_SamplesPerQuad)
    {
    case 1:
    {
        return (readPos.x & 1) | (readPos.y & 1) == 0;
    }
    case 2:
    {
        return (readPos.x & 1) == (readPos.y & 1);
    }
    default:
        // 4
        return true;

    }

}

void IncrementRayCounter(UINT waveRayCount, out UINT baseRayIndex)
{
    RWStructuredBuffer<UINT> rayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];;
    InterlockedAdd(rayCounterBuffer[0], waveRayCount, baseRayIndex);
}

void StoreRay(UINT rayIndex, UINT2 rayCoord, bool copyHorizontal, bool copyVertical, bool copyDiagonal)
{
    RWStructuredBuffer<UINT> rayListBuffer = ResourceDescriptorHeap[g_RayListBufferIndex];
    rayListBuffer[rayIndex] = PackRayData(rayCoord, copyHorizontal, copyVertical, copyDiagonal);
}

float4 CalcSpecular(UINT2 readPos, float Roughness)
{
    float3 dimensions = GetTexture2DDimensions(SkyboxTexIndex);

    const UINT MAX_REFLECTION_LOD = dimensions.z;
    float lod = Roughness * MAX_REFLECTION_LOD;

    float2 sampleUV = ((float2)readPos + 0.5f) * g_DestSize.zw;
    float sampleDepth = LoadTexture2D(OpaqueDepthIndex, readPos);

    float3 normalWS = DecodeNormal(LoadTexture2D(GBuffer3Index, readPos));
    float3 positionWS = ComputeWorldSpacePosition(sampleUV, sampleDepth, viewProjMatrix_I);
    float3 viewDirWS = normalize(cameraPosWS - positionWS);

    float3 R = 2 * dot(viewDirWS, normalWS) * normalWS - viewDirWS;
    float4 specularIBL = SampleTextureCube_LOD(SkyboxTexIndex, R, ClampLinearSampler, lod);

    return specularIBL;
}

void ClearRayCounter()
{
    RWStructuredBuffer<UINT> rayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    rayCounterBuffer[0] = 0;
}

void DoTileClassify(UINT2 readPos, UINT2 groupThreadID, float roughness)
{
    g_TileCount = 0;
    UINT2 writePos = readPos;

    bool isFirstActiveThreadInWave = WaveIsFirstLane();

    // Disable offscreen pixels
    bool needRay = !(readPos.x >= g_DestSize.x || readPos.y >= g_DestSize.y);

    // Disable shoot a ray if very far
    // Disable shoot a ray on very rough surfaces
    bool isReflectiveSurface = IsReflectiveSurface(readPos);
    bool isGlossyReflection = IsGlossyReflection(roughness);
    needRay = needRay && isReflectiveSurface && isGlossyReflection;

    // sample per quad
    bool isShootRay = IsShootRay(readPos);
    needRay = needRay && isShootRay;

    GroupMemoryBarrierWithGroupSync();

    if (isGlossyReflection && isReflectiveSurface)
    {
        InterlockedAdd(g_TileCount, 1);
    }

    bool isRequiredRay = !needRay;
    bool isCopyHorizontal = (g_SamplesPerQuad != 4) && isShootRay && WaveReadLaneAt(
                                isRequiredRay,
                                WaveGetLaneIndex() ^ 0b01);
    bool isCopyVertical = (g_SamplesPerQuad == 1) && isShootRay && WaveReadLaneAt(
                              isRequiredRay,
                              WaveGetLaneIndex() ^ 0b10);
    bool isCopyDiagonal = (g_SamplesPerQuad == 1) && isShootRay && WaveReadLaneAt(
                              isRequiredRay,
                              WaveGetLaneIndex() ^ 0b11);

    UINT waveRayCount = WaveActiveCountBits(needRay);
    UINT baseRayIndex = 0;
    if (isFirstActiveThreadInWave)
    {
        IncrementRayCounter(waveRayCount, baseRayIndex);
    }
    baseRayIndex = WaveReadLaneFirst(baseRayIndex);

    UINT localRayIndexInWave = WavePrefixCountBits(needRay);
    if (needRay)
    {
        UINT storeRayIndex = baseRayIndex + localRayIndexInWave;
        StoreRay(storeRayIndex, readPos, isCopyHorizontal, isCopyVertical, isCopyDiagonal);
    }

    float4 intersectionOutput = (float4)0;
    // if dont shoot ray for sssr, fall back to environment map
    if (isReflectiveSurface && !isGlossyReflection)
    {
        intersectionOutput = CalcSpecular(readPos, roughness);
    }
    SaveIntersectionOutput(writePos, intersectionOutput);

    GroupMemoryBarrierWithGroupSync(); // Wait until g_TileCount
}

void CalcIntersectArgs()
{
    RWStructuredBuffer<UINT> rayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    RWStructuredBuffer<UINT> intersectionArgsBuffer = ResourceDescriptorHeap[g_IntersectionArgsBufferIndex];

    UINT rayCount = rayCounterBuffer[0];
    intersectionArgsBuffer[0] = (rayCount + 63) / 64;
    intersectionArgsBuffer[1] = 1;
    intersectionArgsBuffer[2] = 1;

    rayCounterBuffer[0] = 0;
    rayCounterBuffer[1] = rayCount;
}