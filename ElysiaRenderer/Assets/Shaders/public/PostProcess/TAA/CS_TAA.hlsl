#include "private\ShadingCommon.hlsl"
#include "private\TAACommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewProjMatrix_I;
    Matrix pre_viewProjMatrix;
    Matrix g_ProjMatrix_I;
    Vector4 g_TAATexSize;

    UINT g_HistoryTexIndex;
    UINT g_CurrTexIndex;
    UINT g_SourceTexIndex;
    UINT g_DestTexIndex;

    float g_FixedBlendWeight;
    float2 g_Jitter;
    float2 g_HistoryJitter;
}

void SampleMinMax3x3(UINT currFrameTexIndex,
                     float2 uv,
                     float2 duv,
                     out float3 minColor,
                     out float3 maxColor,
                     out float3 currColor,
                     out float3 avgColor,
                     out float3 m1,
                     out float3 m2);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CopyRT(uint3 id : SV_DispatchThreadID)
{
    UINT2 writePos = id.xy;
    UINT2 readPos = id.xy;

    float3 sourceColor = LoadTexture2D(g_SourceTexIndex, readPos);
    Elysia_Save_TAA(g_DestTexIndex, writePos, sourceColor);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TAA(uint3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    UINT2 writePos = id.xy;
    if (writePos.x > g_TAATexSize.x || writePos.y > g_TAATexSize.y)
        return;

    float2 screenUV = ((float2)readPos + 0.5f) * g_TAATexSize.zw;
    float2 closetUV = SampleClosestUV3x3(OpaqueDepthIndex, screenUV, g_TAATexSize.zw);

    // float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler);
    // float depth01 = Linear01Depth(rawDepth, g_ZBufferParams);

    float2 velocity = Elysia_Sample_Velocity(closetUV);
    float2 preUV = screenUV - velocity;
    // [branch]
    // if (depth01 > 0.999f)
    // {
    //     float2 cleanUV = screenUV - g_Jitter;
    //     float3 viewDir = ComputeViewSpacePosition(cleanUV, rawDepth, g_ProjMatrix_I);
    //
    //     float4 preNDC = mul(float4(viewDir.xyz, 0.0f), pre_viewProjMatrix);
    //     if (abs(preNDC.w) > 0.0001f)
    //     {
    //         preNDC.xyz /= preNDC.w;
    //     }
    //
    //     preUV = preNDC.xy * float2(0.5f, -0.5f) + 0.5f;
    //     preUV += g_HistoryJitter;
    // }
    float3 historyColor = Elysia_Sample_History(g_HistoryTexIndex, preUV);
    historyColor = ReinhardTonemap(historyColor);
    historyColor = TransformRGB2YCoCg(historyColor);

    float3 minColor, maxColor, currColor, avgColor, m1, m2;
    SampleMinMax3x3(g_CurrTexIndex, screenUV, g_TAATexSize.zw, minColor, maxColor, currColor, avgColor, m1, m2);
    historyColor = VarianceClipBox(m1, m2, 1, historyColor);

    float3 blendColor = lerp(currColor, historyColor, g_FixedBlendWeight);
    blendColor = TransformYCoCg2RGB(blendColor);
    blendColor = InverseReinhardTonemap(blendColor);
    Elysia_Save_TAA(g_DestTexIndex, writePos, blendColor);
}

void SampleMinMax3x3(UINT currFrameTexIndex,
                     float2 uv,
                     float2 duv,
                     out float3 minColor,
                     out float3 maxColor,
                     out float3 currColor,
                     out float3 avgColor,
                     out float3 m1,
                     out float3 m2)
{
    float3 colors[9];
    SampleColor3x3(currFrameTexIndex, uv, duv, colors);

    m1 = m2 = 0;
    minColor = maxColor = colors[0];
    [unroll(9)]
    for (UINT i = 0; i < 9; i ++)
    {
        minColor = min(minColor, colors[i]);
        maxColor = max(maxColor, colors[i]);
        avgColor += colors[i];
        m1 += colors[i];
        m2 += Pow2(colors[i]);
    }
    avgColor *= rcp(9.f);
    currColor = colors[4];
}