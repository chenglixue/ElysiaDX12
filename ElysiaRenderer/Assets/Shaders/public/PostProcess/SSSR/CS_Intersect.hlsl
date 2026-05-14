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
    UINT g_MostDetailedMip;

    UINT g_RayCounterBufferIndex;
    UINT g_RayListBufferIndex;
    UINT g_IntersectionOutputTexIndex;
    UINT g_IntersectionArgsBufferIndex;
    UINT g_HIZTexIndex;
}

UINT GetRayCount();
UINT GetPackData(UINT rayIndex);
float3 GetNormalWS(float2 sampleUV);
float GetRoughness(float2 sampleUV);
bool IsMirror(float roughness);
UINT GetMostDetailedMip();
float GetSSSRDepth(int2 readPos, int mip);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]

void DoIntersect(uint2 groupID : SV_GroupID,
                 uint groupIndex : SV_GroupIndex)
{
    const UINT rayIndex = groupID * GROUP_SIZE * GROUP_SIZE + groupIndex;
    if (rayIndex >= GetRayCount())
        return;

    UINT packData = GetPackData(rayIndex);
    int2 rayPos;
    bool copyHorizontal;
    bool copyVertical;
    bool copyDiagonal;
    UnpackRayData(packData, rayPos, copyHorizontal, copyVertical, copyDiagonal);

    const UINT2 screenSize = g_DestSize.xy;
    const float2 screenUV = ((float2)rayPos + 0.5f) * g_DestSize.zw;

    float3 normalWS = GetNormalWS(screenUV);
    float roughness = GetRoughness(screenUV);
    bool bIsMirror = IsMirror(roughness);

    int mostDetailedMip = bIsMirror ? 0 : GetMostDetailedMip();
    float2 mipResolution = SSSR_GetMipResolution(screenSize, mostDetailedMip);
    float depth = GetSSSRDepth(screenUV * mipResolution, mostDetailedMip);

    float3 positionVS = ComputeViewSpacePosition(screenUV, depth, projMatrix_I);
    float3 rayDirVS = normalize(positionVS);

    float3 normalVS = normalize(mul(float4(normalWS, 0.f), viewMatrix));
}

UINT GetRayCount()
{
    StructuredBuffer<UINT> rayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    return rayCounterBuffer[1];
}

UINT GetPackData(UINT rayIndex)
{
    StructuredBuffer<UINT> rayListBuffer = ResourceDescriptorHeap[g_RayListBufferIndex];
    return rayListBuffer[rayIndex];
}

float3 GetNormalWS(float2 sampleUV)
{
    float3 normalWS = SampleNormalWS(sampleUV, ClampPointSampler);

    return normalWS;
}

float GetRoughness(float2 sampleUV)
{
    return SampleRoughness(sampleUV);
}

bool IsMirror(float roughness)
{
    return roughness < 1e-4;
}

UINT GetMostDetailedMip()
{
    float3 dimensions = GetTexture2DDimensions(SkyboxTexIndex);

    const UINT MAX_REFLECTION_LOD = dimensions.z;

    return MAX_REFLECTION_LOD;
}

float GetSSSRDepth(int2 readPos, int mip)
{
    Texture2D<float> HIZTex = ResourceDescriptorHeap[g_HIZTexIndex];
    return HIZTex.Load(readPos, mip);
}