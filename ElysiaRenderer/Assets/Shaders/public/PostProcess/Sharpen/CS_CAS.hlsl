#include "private\ShadingCommon.hlsl"
#include "private\CASCommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_SharpenTexSize;

    UINT g_SharpenTexIndex;
    float g_SharpenIntensity;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CAS(uint3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    UINT2 writePos = id.xy;
    if (writePos.x >= (uint)g_SharpenTexSize.x || writePos.y >= (uint)g_SharpenTexSize.y)
        return;

    float2 uv = (readPos + 0.5f) * g_SharpenTexSize.zw;
    float2 duv = g_SharpenTexSize.zw;

    float3 bottomColor = SampleTexture2D(g_SharpenTexIndex, uv + int2(0, -1) * duv, ClampPointSampler);
    float3 leftColor = SampleTexture2D(g_SharpenTexIndex, uv + int2(-1, 0) * duv, ClampPointSampler);
    float3 middleColor = SampleTexture2D(g_SharpenTexIndex, uv + int2(0, 0) * duv, ClampPointSampler);
    float3 rightColor = SampleTexture2D(g_SharpenTexIndex, uv + int2(1, 0) * duv, ClampPointSampler);
    float3 topColor = SampleTexture2D(g_SharpenTexIndex, uv + int2(0, 1) * duv, ClampPointSampler);

    float3 minColor = min(min(min(bottomColor, leftColor), min(middleColor, rightColor)), topColor);
    float3 maxColor = max(max(max(bottomColor, leftColor), max(middleColor, rightColor)), topColor);

    // maxColor 越接近 1.0 或 minColor 越接近 0.0（高对比度），amp 越小，锐化越弱
    float3 dir = min(minColor, 1.0f - maxColor);

    float3 mx_safe = max(maxColor, 1e-5f);
    float3 ampSq = saturate(dir / mx_safe);
    float3 amp = sqrt(ampSq);

    // 映射曲线：Sharpness 从 0.0 -> 1.0，对应的峰值从 -1/8 -> -1/5
    float peak = -lerp(8.0f, 5.0f, saturate(g_SharpenIntensity));
    float3 w = amp / peak;

    float3 rcpWeight = 1.0f / (1.0f + 4.0f * w);
    float3 finalColor = saturate(
        (bottomColor * w + leftColor * w + rightColor * w + topColor * w + middleColor) * rcpWeight);

    Elyisa_CAS_Save(g_SharpenTexIndex, writePos, finalColor);
}