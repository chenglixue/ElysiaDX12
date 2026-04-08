#include "private\ShadingCommon.hlsl"
#include "private\TAACommon.hlsli"

#define GROUP_SIZE 8
#define TILE_SIZE (GROUP_SIZE + 2)
#define TILE_PIXELS (TILE_SIZE * TILE_SIZE)

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix pre_viewProjMatrix;
    Matrix g_ProjMatrix_I;
    Vector4 g_TAATexSize;
    Vector4 g_DownSampleTexSize;

    UINT g_HistoryTexIndex;
    UINT g_CurrTexIndex;
    UINT g_SourceTexIndex;
    UINT g_DestTexIndex;

    float g_StaticBlendWeight;
    float g_DynamicBlendWeight;
    float g_MaxBlendWeight;
    float g_UpScaleFactor;

    float2 g_Jitter;
    float2 g_HistoryJitter;
    float2 g_JitterPixels;
}

// groupshared float4 g_LDSColorDepth[TILE_PIXELS];

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
    if (writePos.x >= g_TAATexSize.x || writePos.y >= g_TAATexSize.y)
        return;

    if (g_UpScaleFactor == 1)
    {
        float2 screenUV = ((float2)readPos + 0.5f) * g_TAATexSize.zw;
        float2 closetUV = SampleClosestUV3x3(OpaqueDepthIndex, screenUV, g_TAATexSize.zw);

        float2 velocity = Elysia_Sample_Velocity(closetUV);
        float2 preUV = screenUV - velocity;
        float3 historyColor = CatmullRomSample(g_HistoryTexIndex, preUV, g_TAATexSize.zw);
        historyColor = ReinhardTonemap(historyColor);
        historyColor = TransformRGB2YCoCg(historyColor);

        float3 minColor, maxColor, currColor, avgColor, m1, m2;
        SampleMinMax3x3(g_CurrTexIndex, screenUV, g_TAATexSize.zw, minColor, maxColor, currColor, avgColor, m1, m2);
        historyColor = VarianceClipBox(m1, m2, 1, historyColor);

        float velocityFactor = length(velocity) * g_TAATexSize.xy;
        float blendWeight = CalcTAAWeight(g_StaticBlendWeight, g_DynamicBlendWeight, g_MaxBlendWeight, velocityFactor);

        float3 blendColor = lerp(currColor, historyColor, blendWeight);
        blendColor = TransformYCoCg2RGB(blendColor);
        blendColor = InverseReinhardTonemap(blendColor);
        Elysia_Save_TAA(g_DestTexIndex, writePos, blendColor);
    }
    else
    {
        float2 upSampleUV = ((float2)readPos + 0.5f) * g_TAATexSize.zw;
        // float2 closetUV = SampleClosestUV3x3(OpaqueDepthIndex, upSampleUV, g_TAATexSize.zw);
        float2 closetUV = 0.f;
        // include jitter
        float2 downSampleJiiterPos = upSampleUV * g_DownSampleTexSize.xy + g_JitterPixels / g_UpScaleFactor;
        // 离downSamplePos最近的pixel中心
        float2 centerDownSamplePos = floor(downSampleJiiterPos) + 0.5f;
        // down sample pixel offset
        float2 posCenterToJitter = downSampleJiiterPos - centerDownSamplePos;

        float3 minColor, maxColor, currColor, avgColor, m1, m2;
        DownSample3x3(g_CurrTexIndex,
                      centerDownSamplePos,
                      g_DownSampleTexSize,
                      posCenterToJitter,
                      g_UpScaleFactor,
                      currColor,
                      minColor,
                      maxColor,
                      m1,
                      m2,
                      closetUV);

        float2 velocity = Elysia_Sample_Velocity(upSampleUV);
        float2 preUV = upSampleUV - velocity;
        if (any(preUV < 0.f) || any(preUV > 1.f))
        {
            float3 finalRGB = InverseReinhardTonemap(TransformYCoCg2RGB(currColor));
            Elysia_Save_TAA(g_DestTexIndex, writePos, finalRGB);
            return;
        }
        float3 historyColor = CatmullRomSample(g_HistoryTexIndex, preUV, g_TAATexSize.zw);
        historyColor = max(0.0f, historyColor);
        historyColor = ReinhardTonemap(historyColor);
        historyColor = TransformRGB2YCoCg(historyColor);

        // SampleMinMax3x3(g_CurrTexIndex, upSampleUV, g_TAATexSize.zw, minColor, maxColor, currColor, avgColor, m1, m2);
        historyColor = TAAUVarianceClipBox(m1, m2, 1, historyColor);

        float velocityFactor = length(velocity) * g_TAATexSize.xy;
        float blendWeight = CalcTAAWeight(g_StaticBlendWeight, g_DynamicBlendWeight, g_MaxBlendWeight, velocityFactor);
        float spatialConfidence = ComputeTAAUWeight(posCenterToJitter, g_UpScaleFactor);
        // blendWeight = saturate(blendWeight * (1.0f - spatialConfidence * 0.1f));

        float3 blendColor = lerp(currColor, historyColor, blendWeight);
        blendColor = TransformYCoCg2RGB(blendColor);
        blendColor = InverseReinhardTonemap(blendColor);
        blendColor = max(0.0f, blendColor);
        Elysia_Save_TAA(g_DestTexIndex, writePos, blendColor);
    }

}