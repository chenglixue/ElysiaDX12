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

float4 GatherRedTexture2D(UINT textureIndex, float2 uv, UINT samplerStateIndex)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];

    return SampleTex.GatherRed(Sampler, uv);
}

float4 GatherGreenTexture2D(UINT textureIndex, float2 uv, UINT samplerStateIndex)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];

    return SampleTex.GatherGreen(Sampler, uv);
}

float4 GatherBlueTexture2D(UINT textureIndex, float2 uv, UINT samplerStateIndex)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];

    return SampleTex.GatherBlue(Sampler, uv);
}

float4 GatherAlphaTexture2D(UINT textureIndex, float2 uv, UINT samplerStateIndex)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];
    SamplerState Sampler = SamplerDescriptorHeap[samplerStateIndex];

    return SampleTex.GatherAlpha(Sampler, uv);
}

float4 LoadTexture2D(UINT textureIndex, int2 coord)
{
    Texture2D<float4> SampleTex = ResourceDescriptorHeap[textureIndex];

    return SampleTex.Load(int3(coord, 0));
}
#endif