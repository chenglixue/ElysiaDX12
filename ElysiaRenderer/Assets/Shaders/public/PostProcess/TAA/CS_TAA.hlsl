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

    float2 upSampleUV = ((float2)readPos + 0.5f) * g_TAATexSize.zw;
    // float2 closetUV = SampleClosestUV3x3(OpaqueDepthIndex, upSampleUV, g_TAATexSize.zw);
    float2 closetUV;
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

    float2 velocity = Elysia_Sample_Velocity(closetUV);
    float2 preUV = upSampleUV - velocity;
    if (any(preUV < 0.f) || any(preUV > 1.f))
    {
        float3 finalRGB = InverseReinhardTonemap(TransformYCoCg2RGB(currColor));
        Elysia_Save_TAA(g_DestTexIndex, writePos, finalRGB);
        return;
    }
    float3 historyColor = CatmullRomSample(g_HistoryTexIndex, preUV, g_TAATexSize.zw);
    if (any(isnan(historyColor)) || any(isinf(historyColor)))
    {
        historyColor = currColor;
    }
    historyColor = max(0.0f, historyColor);
    historyColor = ReinhardTonemap(historyColor);
    historyColor = TransformRGB2YCoCg(historyColor);

    // SampleMinMax3x3(g_CurrTexIndex, upSampleUV, g_TAATexSize.zw, minColor, maxColor, currColor, avgColor, m1, m2);
    historyColor = TAAUVarianceClipBox(m1, m2, 2, historyColor);

    float currRawDepth = SampleTexture2D(OpaqueDepthIndex, closetUV, ClampPointSampler);
    float currLinear01Depth = Linear01Depth(currRawDepth, g_ZBufferParams);
    float preRawDepth = SampleTexture2D(OpaqueDepthIndex, preUV, ClampPointSampler);
    float preLinear01Depth = Linear01Depth(preRawDepth, g_ZBufferParams);
    float depth01Diff = abs(currLinear01Depth - preLinear01Depth) / (
                            max(currLinear01Depth, preLinear01Depth) + FLT_EPS);
    float depthDiffThreshold = 0.03f;
    float depthPenalty = 1.0f - smoothstep(depthDiffThreshold * 0.5f, depthDiffThreshold, depth01Diff);

    float velocityFactor = length(velocity) * g_TAATexSize.xy;
    float blendWeight = CalcTAAWeight(g_StaticBlendWeight, g_DynamicBlendWeight, g_MaxBlendWeight, velocityFactor);
    float spatialConfidence = ComputeTAAUWeight(posCenterToJitter, g_UpScaleFactor);
    blendWeight = saturate(blendWeight * (1.0f - spatialConfidence * 0.5f));
    blendWeight *= depthPenalty;

    float3 blendColor = lerp(currColor, historyColor, blendWeight);
    blendColor = TransformYCoCg2RGB(blendColor);
    blendColor = InverseReinhardTonemap(blendColor);
    blendColor = max(0.0f, blendColor);
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