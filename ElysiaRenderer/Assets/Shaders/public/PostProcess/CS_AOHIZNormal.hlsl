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

float MipSmartAverage(float4 depths, float effectRadius);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AOHIZNormal(UINT3 id : SV_DispatchThreadID)
{
    const UINT layerIndex = id.z;
    const UINT2 targetCoord = id.xy;
    const UINT2 srcCoord = id.xy * 2;

    if (targetCoord.x >= (uint)g_TargetSize.x || targetCoord.y >= (uint)g_TargetSize.y)
        return;

    UINT2 offset[4] =
    {
        uint2(0, 0),
        uint2(1, 0),
        uint2(0, 1),
        uint2(1, 1),
    };

    float4 depths;
    for (UINT i = 0; i < 4; i ++)
    {
        depths[i] = LoadTexture2D(g_SourceTexIndices[layerIndex], srcCoord + offset[i]) *
                    Constant_Float16F_Scale;
    }

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];

    o[id.xy] = MipSmartAverage(depths, g_AORadius) / Constant_Float16F_Scale;
}

float MipSmartAverage(float4 depths, float effectRadius)
{
    float closest = min(min(depths[0], depths[1]), min(depths[2], depths[3]));
    float falloffCalcMulSq = -1.0f / (effectRadius * effectRadius);

    float4 dists = depths - closest.xxxx;
    float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);

    return dot(weights, depths) / dot(weights, float4(1, 1, 1, 1));
}