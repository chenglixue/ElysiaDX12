#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_SourceSize;
    UINT g_SourceTexIndex;
    UINT g_TargetTexIndex;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void DownSampleEyeDepth(UINT3 id : SV_DispatchThreadID)
{
    const uint2 srcBaseCoord = id.xy * 2;
    float2 screenUV = (srcBaseCoord + 1.0f) * g_SourceSize.zw;

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    float minDepth = FLT_MAX;

    float4 gatherDepth = GatherRedTexture2D(g_SourceTexIndex, screenUV, ClampPointSampler);
    [unroll(4)]
    for (int i = 0; i < 4; i ++)
    {
        minDepth = min(minDepth, gatherDepth[i]);
    }

    minDepth = LinearEyeDepth(minDepth, g_ZBufferParams);
    minDepth /= Constant_Float16F_Scale;
    o[id.xy] = minDepth;
}

float MipSmartAverage(float4 depths, float effectRadius)
{
    float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
    float falloffCalcMulSq = -1.0f / (effectRadius * effectRadius);

    float4 dists = depths - closest.xxxx;
    float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);

    return dot(weights, depths) / dot(weights, float4(1, 1, 1, 1));
}