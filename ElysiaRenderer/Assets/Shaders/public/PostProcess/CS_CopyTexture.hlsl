#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CopyTexture(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    Texture2D<float> srcTex = ResourceDescriptorHeap[OpaqueDepthIndex];

    o[dispatchThreadID.xy] = srcTex.Load(uint3(dispatchThreadID.xy, 0));
}