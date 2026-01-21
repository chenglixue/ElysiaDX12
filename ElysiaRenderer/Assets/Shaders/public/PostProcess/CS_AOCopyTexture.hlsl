#include "private\ShadingCommon.hlsl"
#include "private/SSAOCommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    float4 g_TargetSize;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CopyTexture(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    float3 normalWS = SampleNormalWS(screenUV,ClampPointSampler);
    float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler);
    float eyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);
    eyeDepth /= Constant_Float16F_Scale;

    float4 result = float4(normalWS, eyeDepth);
    o[dispatchThreadID.xy] = result;
}