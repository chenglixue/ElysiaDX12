#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_SourceSize;
    float4 g_InputViewportMaxBound;
    UINT g_GBufferHIZSourceTexIndex;
    UINT g_GBufferHIZTargetTexIndex;
}

void SaveHIZ(UINT2 writePos, float depth)
{
    RWTexture2D<float> HIZTex = ResourceDescriptorHeap[g_GBufferHIZTargetTexIndex];
    HIZTex[writePos].r = depth;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void GBuffer_Copy_Depth(uint3 GlobalID : SV_DispatchThreadID)
{
    UINT2 writePos = GlobalID.xy;
    UINT2 readPos = writePos;

    if (writePos.x >= (uint)g_TargetSize.x || writePos.y >= (uint)g_TargetSize.y)
        return;

    float2 sampleUV = ((float2)readPos + 0.5f) * g_TargetSize.zw;
    sampleUV = min(sampleUV - float2(0.25f, 0.25f) * g_SourceSize.zw, g_InputViewportMaxBound.xy - g_SourceSize.zw);
    float4 depths = GatherRedTexture2D(OpaqueDepthIndex, sampleUV, ClampPointSampler);
    float maxDepth = max(max(depths.x, depths.y), max(depths.z, depths.w));

    SaveHIZ(writePos, maxDepth);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void GBuffer_HIZ(uint3 GlobalID : SV_DispatchThreadID)
{
    UINT2 readPos = GlobalID.xy * 2;
    UINT2 writePos = GlobalID.xy;
    uint2 maxPos = (uint2)g_SourceSize.xy - 1;

    if (writePos.x >= (uint)g_TargetSize.x || writePos.y >= (uint)g_TargetSize.y)
        return;

    float depths[4];
    UINT2 offsets[4] =
    {
        UINT2(0, 0),
        UINT2(0, 1),
        UINT2(1, 0),
        UINT2(1, 1)
    };
    float maxDepth = 0.f;
    [unroll(4)]
    for (UINT i = 0; i < 4; i ++)
    {
        depths[i] = LoadTexture2D(g_GBufferHIZSourceTexIndex, min(readPos + offsets[i], maxPos));
        maxDepth = max(maxDepth, depths[i]);
    }

    SaveHIZ(writePos, maxDepth);
}