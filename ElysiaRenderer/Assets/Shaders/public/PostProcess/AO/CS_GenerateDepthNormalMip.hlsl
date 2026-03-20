#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4x4 viewMatrix;
    float4 g_TargetSize;
    float4 g_SourceSize;

    Vector4 g_TargetDepthTexIndices;
    Vector4 g_SourceDepthTexIndices;
    Vector4 g_DeinterLeaveDepthTexIndices;
    Vector4 g_DeinterLeaveNormalTexIndices;

    float g_AORadius;
    UINT g_SourceTexIndex;
    UINT g_SourceDepthTexIndex;
    UINT g_SourceNormalTexIndex;
    float2 g_DepthUnpackConsts;
}

float MipSmartAverage(float4 depths, float effectRadius);
void Store_Depth(UINT textureIndex, UINT2 id, float depth);
void Store_Normal(UINT textureIndex, UINT2 id, float3 normal);
float ScreenSpaceToViewSpaceDepth(float screenDepth);
void DownSampleDeinterleaveNormalDepth(UINT2 id);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AODeinterleavedHIZ(UINT3 id : SV_DispatchThreadID)
{
    const UINT layerIndex = id.z;
    const UINT2 targetCoord = id.xy;
    const UINT2 srcCoord = id.xy * 2;

    if (targetCoord.x >= (uint)g_TargetSize.x || targetCoord.y >= (uint)g_TargetSize.y)
        return;
    float2 screenUV = ((float2)srcCoord + 0.5f * 2) * g_SourceSize.zw;

    float4 depths = GatherRedTexture2D(g_SourceDepthTexIndices[layerIndex],
                                       screenUV,
                                       ClampPointSampler);

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetDepthTexIndices[layerIndex]];

    o[id.xy] = MipSmartAverage(depths, g_AORadius);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void DeinterleaveNormalDepth(UINT3 id : SV_DispatchThreadID)
{
    const UINT2 targetCoord = id.xy;
    const UINT2 srcCoord = id.xy << 2;
    const UINT2 offset[4] = {int2(0, 0), int2(2, 0), int2(0, 2), int2(2, 2)};

    [unroll(4)]
    for (UINT i = 0; i < 4; i ++)
    {
        UINT2 sampleCoord = srcCoord + offset[i];

        float sampleDepth = LoadTexture2D(g_SourceDepthTexIndex, sampleCoord);
        float3 sampleNormal = LoadTexture2D(g_SourceNormalTexIndex, sampleCoord);
        sampleNormal = DecodeNormal(sampleNormal);

        sampleDepth = ScreenSpaceToViewSpaceDepth(sampleDepth);
        sampleNormal = normalize(mul(sampleNormal, (float3x3)viewMatrix));
        sampleNormal = EncodeNormal(sampleNormal);

        Store_Depth(g_DeinterLeaveDepthTexIndices[i], targetCoord, sampleDepth);
        Store_Normal(g_DeinterLeaveNormalTexIndices[i], targetCoord, sampleNormal);
    }
}

float MipSmartAverage(float4 depths, float effectRadius)
{
    float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
    float falloffCalcMulSq = -1.0f / (effectRadius * effectRadius);

    float4 dists = depths - closest.xxxx;
    float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);

    return dot(weights, depths) / dot(weights, float4(1, 1, 1, 1));
}

void Store_Depth(UINT textureIndex, UINT2 id, float depth)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[textureIndex];
    o[id].r = depth;
}
void Store_Normal(UINT textureIndex, UINT2 id, float3 normal)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[textureIndex];
    o[id].rgb = normal;
}

float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
    float depthLinearizeMul = g_DepthUnpackConsts.x;
    float depthLinearizeAdd = g_DepthUnpackConsts.y;

    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

void NativeDeinterleaveNormalDepth(UINT2 id)
{
    // pixel pos in Deinterleave tex
    UINT2 writePos = id.xy;
    UINT2 readPos = writePos * 2;

    // [0,0]、[0,1]、[1,0]、[1,1] in 2x2
    UINT2 pixelOffset = readPos % 2;
    // Deinterleave tex index
    UINT layerIndex = pixelOffset.x + pixelOffset.y * 2;

    float eyeDepth = LoadTexture2D(g_SourceDepthTexIndex, readPos);
    eyeDepth = ScreenSpaceToViewSpaceDepth(eyeDepth);
    float3 sampleNormal = LoadTexture2D(g_SourceNormalTexIndex, readPos);

    Store_Depth(g_DeinterLeaveDepthTexIndices[layerIndex], writePos, eyeDepth);
    Store_Normal(g_DeinterLeaveNormalTexIndices[layerIndex], writePos, sampleNormal);
}
void DownSampleDeinterleaveNormalDepth(UINT2 id)
{
    const UINT2 targetCoord = id.xy;
    const UINT2 srcCoord = id.xy << 2;

    const UINT2 offset[4] = {int2(0, 0), int2(2, 0), int2(0, 2), int2(2, 2)};

    [unroll(4)]
    for (UINT i = 0; i < 4; i ++)
    {
        UINT2 sampleCoord = srcCoord + offset[i];

        float sampleDepth = LoadTexture2D(g_SourceDepthTexIndex, sampleCoord);
        float3 sampleNormal = LoadTexture2D(g_SourceNormalTexIndex, sampleCoord);
        sampleNormal = DecodeNormal(sampleNormal);

        sampleDepth = ScreenSpaceToViewSpaceDepth(sampleDepth);
        sampleNormal = mul(sampleNormal, (float3x3)viewMatrix);
        sampleNormal = EncodeNormal(sampleNormal);

        Store_Depth(g_DeinterLeaveDepthTexIndices[i], targetCoord, sampleDepth);
        Store_Normal(g_DeinterLeaveNormalTexIndices[i], targetCoord, sampleNormal);
    }
}