#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float4 g_TargetSize;
    float4 g_SourceSize;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TwoTwoMinHIZ(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= UINT(g_TargetSize.x) || dispatchThreadID.y >= UINT(g_TargetSize.y))
    {
        return;
    }

    Texture2D<float> srcTex = ResourceDescriptorHeap[g_SourceTexIndex];
    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_SourceSize.zw;
    UINT2 destCoord = dispatchThreadID.xy;
    UINT2 srcCoord = dispatchThreadID.xy * 2;
    uint2 maxCoord = (uint2)g_SourceSize.xy - 1;

    float depthArray[4];
    depthArray[0] = srcTex[min(srcCoord + UINT2(0, 0), maxCoord)].r;
    depthArray[1] = srcTex[min(srcCoord + UINT2(0, 1), maxCoord)].r;
    depthArray[2] = srcTex[min(srcCoord + UINT2(1, 0), maxCoord)].r;
    depthArray[3] = srcTex[min(srcCoord + UINT2(1, 1), maxCoord)].r;

    float minDepth = FLT_MAX;
    [unroll(4)]
    for (UINT i = 0; i < 4; i ++)
    {
        minDepth = min(minDepth, depthArray[i]);
    }

    o[destCoord] = minDepth;
}