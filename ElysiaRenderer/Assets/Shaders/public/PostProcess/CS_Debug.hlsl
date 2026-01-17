#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

#define DEBUG_NONE 0
#define DEBUG_AO 1

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    UINT g_DebugMode;
    UINT g_MipmapLevel;
    float4 g_SourceSize;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void Debug(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    Texture2D<float> srcTex = ResourceDescriptorHeap[g_SourceTexIndex];

    float2 screenUV = (dispatchThreadID.xy + 0.5f) * g_SourceSize.zw;

    switch (g_DebugMode)
    {
    case DEBUG_NONE:
    {
        return;
    }
    case DEBUG_AO:
    {
        float4 rawDepth = SampleTexture2D_LOD(g_SourceTexIndex, screenUV, ClampPointSampler, g_MipmapLevel);
        o[dispatchThreadID.xy] = Linear01Depth(rawDepth, g_ZBufferParams);
        o[dispatchThreadID.xy] = SampleTexture2D(g_SourceTexIndex, screenUV, ClampPointSampler);
    }
    }
}