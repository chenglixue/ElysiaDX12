#ifndef Deferred_Common_H
#define Deferred_Common_H

#include "SharedCommon.hlsli"

float3 GetDeferredAlbedo(float2 screenUV)
{
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    Texture2D<float3> baseColorTex = ResourceDescriptorHeap[GBuffer0Index];
    
    float3 o = baseColorTex.Sample(warpLinearSampler, screenUV);

    return o;
}

float GetDeferredOpacity(float2 screenUV)
{
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[GBuffer4Index];
    
    float3 o = baseColorTex.Sample(warpLinearSampler, screenUV).a;

    return o;
}


#endif