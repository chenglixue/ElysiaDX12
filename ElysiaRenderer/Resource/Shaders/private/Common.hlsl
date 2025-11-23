#ifndef COMMON_H
#define COMMON_H

#pragma once

#include "SharedCommon.hlsli"

float4 SampleTexture2D(UINT textureIndex, float2 uv, UINT samplerStateIndex)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];
    
    return SampleTex.Sample(Sampler, uv);
}
#endif