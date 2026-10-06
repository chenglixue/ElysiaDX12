#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_SourceSize;

    Vector4 g_TargetTexIndices;
    Vector4 g_SourceTexIndices;

    float g_AORadius;
}

float MipSmartAverage(float4 depths, min16float effectRadius);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AOHIZNormal(UINT3 id : SV_DispatchThreadID)
{
    const UINT layerIndex = id.z;
    const UINT2 targetCoord = id.xy;
    const UINT2 srcCoord = id.xy * 2;

    bool isEvenGroup = (layerIndex == 0 || layerIndex == 3);
    bool isCurrentFrameActive = (isEvenGroup == (frameIndex % 2 == 0));
    if (!isCurrentFrameActive)
    {
        return;
    }

    if (targetCoord.x >= (uint)g_TargetSize.x || targetCoord.y >= (uint)g_TargetSize.y)
        return;

    float2 screenUV = (srcCoord + 1.f) * g_SourceSize.zw;

    float4 depths = GatherRedTexture2D(g_SourceTexIndices[layerIndex],
                                       screenUV,
                                       ClampPointSampler);

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];

    o[id.xy] = MipSmartAverage(depths, g_AORadius);
}

float MipSmartAverage(float4 depths, min16float effectRadius)
{
    float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
    min16float falloffCalcMulSq = -1.0f / (effectRadius * effectRadius);

    float4 dists = depths - closest.xxxx;
    float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);

    return dot(weights, depths) / dot(weights, float4(1, 1, 1, 1));
}