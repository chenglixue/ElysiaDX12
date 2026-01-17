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

float4 SampleTexture2D_LOD(UINT textureIndex, float2 uv, UINT samplerStateIndex, float LOD)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];

    return SampleTex.SampleLevel(Sampler, uv, LOD);
}

float4 LoadTexture2D(UINT textureIndex, int2 coord)
{
    RWTexture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];

    return SampleTex.Load(int3(coord, 0));
}
#endif