#ifndef BLOOM_COMMON_H
#define BLOOM_COMMON_H
#include "ShadingCommon.hlsl"

#define BLOOM_MIPMAP_COUNT 6
void Elysia_Store_Bloom(float2 writePos, UINT texIndex, float3 color)
{
    RWTexture2D<float3> o = ResourceDescriptorHeap[texIndex];
    o[writePos] = color;
}

float3 Elysia_Load_Bloom(float2 readPos, UINT texIndex)
{
    RWTexture2D<float3> o = ResourceDescriptorHeap[texIndex];
    return o[readPos];
}

float GetLuminanceWeight(float luminance)
{
    return rcp(1.f + luminance);
}
float GetLuminanceWeight(float3 color)
{
    float luminance = Luminance(max(color, 0.f));

    return rcp(1.f + luminance);
}

float GetKarisWeight(float3 color)
{
    return rcp(1.f + Luminance(max(color, 0.f)));
}

float3 SafeHDR(float3 c)
{
    float3 sanitized = clamp(c, 0.0, 65000.0);
    return any(isnan(sanitized)) ? 0.0f : sanitized;
}
#endif