#ifndef TAACOMMON_H
#define TAACOMMON_H

#include "private\ShadingCommon.hlsl"

float2 Elysia_Sample_Velocity(float2 uv)
{
    float2 velocity = SampleTexture2D(GBuffer5Index, uv, ClampPointSampler);
    return velocity;
}
float3 Elysia_Sample_History(UINT historyTexIndex, float2 preUV)
{
    float3 historyColor = SampleTexture2D(historyTexIndex, preUV, ClampLinearSampler);
    return historyColor;
}

float3 Elysia_Sample_TAA(UINT currTexIndex, float2 uv)
{
    float3 o = SampleTexture2D(currTexIndex, uv, ClampLinearSampler);
    return o;
}
void Elysia_Save_TAA(UINT currTexIndex, UINT2 writePos, float3 finalColor)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[currTexIndex];
    o[writePos] = float4(finalColor, 1.f);
}

float3 TransformRGB2YCoCg(float3 c)
{
    // Y  = R/4 + G/2 + B/4
    // Co = R/2 - B/2
    // Cg = -R/4 + G/2 - B/4
    return float3(
        c.x / 4.0 + c.y / 2.0 + c.z / 4.0,
        c.x / 2.0 - c.z / 2.0,
        -c.x / 4.0 + c.y / 2.0 - c.z / 4.0
        );
}
float3 TransformYCoCg2RGB(float3 c)
{
    // R = Y + Co - Cg
    // G = Y + Cg
    // B = Y - Co - Cg
    return saturate(float3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
        ));
}

float3 ReinhardTonemap(float3 color)
{
    return color * rcp(1.0 + Luminance(clamp(color, 0.f, 0.9999f)) + FLT_EPS);
}
float3 InverseReinhardTonemap(float3 color)
{
    color = clamp(color, 0.f, 0.9999f);
    return color * rcp(1.f - Luminance(color) + FLT_EPS);
}

void SampleDepth3x3(UINT depthTexIndex,
                    float2 uv,
                    float2 duv,
                    out float depths[9])
{
    float du = duv.x;
    float dv = duv.y;

    const float2 offsetUV[9] =
    {
        {-du, dv}, {0, dv}, {du, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {-du, -dv}, {0, -dv}, {du, -dv}
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float depth = SampleTexture2D(depthTexIndex, uv + offsetUV[i], ClampPointSampler);
        depths[i] = depth;
    }
}
void SampleDepthCross(UINT depthTexIndex,
                      float2 uv,
                      float2 duv,
                      out float depths[5])
{
    float du = duv.x;
    float dv = duv.y;

    const float2 offsetUV[5] =
    {
        {0, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {0, -dv},
    };

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        float depth = SampleTexture2D(depthTexIndex, uv + offsetUV[i], ClampPointSampler);
        depths[i] = depth;
    }
}


void SampleColor3x3(UINT colorTexIndex,
                    float2 uv,
                    float2 duv,
                    out float3 colors[9])
{
    float du = duv.x;
    float dv = duv.y;

    const float2 offsetUV[9] =
    {
        {-du, dv}, {0, dv}, {du, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {-du, -dv}, {0, -dv}, {du, -dv}
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float3 color = SampleTexture2D(colorTexIndex, uv + offsetUV[i], ClampLinearSampler);
        colors[i] = TransformRGB2YCoCg(ReinhardTonemap(color));
    }
}
void SampleColorCross(UINT colorTexIndex,
                      float2 uv,
                      float2 duv,
                      out half3 colors[5])
{
    float du = duv.x;
    float dv = duv.y;

    const float2 offsetUV[5] =
    {
        {0, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {0, -dv},
    };

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        half3 color = SampleTexture2D(colorTexIndex, uv + offsetUV[i], ClampLinearSampler);
        colors[i] = TransformRGB2YCoCg(ReinhardTonemap(color));
    }
}

float2 SampleClosestUV3x3(UINT depthTexIndex, float2 uv, float2 duv)
{
    float depths[9];
    SampleDepth3x3(depthTexIndex, uv, duv, depths);

    float du = duv.x;
    float dv = duv.y;

    float minDepth = depths[4];
    float2 minUV = uv;
    const float2 offsetUV[9] =
    {
        {-du, dv}, {0, dv}, {du, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {-du, -dv}, {0, -dv}, {du, -dv}
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        const float lerpFactor = step(depths[i].r, minDepth);
        if (minDepth > depths[i].r)
        {
            minDepth = depths[i];
            minUV = uv + offsetUV[i];
        }

        // minDepth = lerp(minDepth, depths[i], lerpFactor);
        // minUV = lerp(minUV, uv + offsetUV[i], lerpFactor);
    }

    return minUV;
}
float2 SampleClosestUVCross(UINT depthTexIndex, float2 uv, float2 duv)
{
    float depths[5];
    SampleDepthCross(depthTexIndex, uv, duv, depths);

    float du = duv.x;
    float dv = duv.y;

    float minDepth = FLT_MAX;
    float2 minUV = uv;
    const float2 offsetUV[5] =
    {
        {0, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {0, -dv},
    };

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        const float lerpFactor = step(depths[i].r, minDepth);

        minDepth = lerp(minDepth, depths[i], lerpFactor);
        minUV = lerp(minUV, minUV + offsetUV[i], lerpFactor);
    }

    return minUV;
}


float3 ClampBox(float3 historyColor, float3 minColor, float3 maxColor)
{
    return clamp(historyColor, minColor, maxColor);
}
float3 ClipBox(float3 currColor, float3 minColor, float3 maxColor)
{
    float3 midColor = (minColor + maxColor) * 0.5;
    float3 toEdgeVec = (maxColor - minColor) * 0.5;

    float3 toSrcVec = currColor - midColor;
    float3 unitVec = abs(toSrcVec / max(toEdgeVec, FLT_EPS));
    float unit = max(unitVec.x, max(unitVec.y, max(unitVec.z, FLT_EPS)));
    float3 res = lerp(currColor, midColor + toSrcVec * rcp(unit), step(1.0, unit));

    return res;
}
float3 VarianceClipBox(float3 m1, float3 m2, float gamma, float3 preColor)
{
    float3 mu = m1 / 9;
    float3 sigma = sqrt(abs(m2 / 9 - mu * mu));
    sigma += FLT_EPS;
    float3 colorMin = mu - gamma * sigma;
    float3 colorMax = mu + gamma * sigma;

    colorMin.x = max(colorMin.x, 0.0f);

    float3 p_clip = 0.5 * (colorMax + colorMin);
    float3 e_clip = 0.5 * (colorMax - colorMin) + FLT_EPS;

    float3 v_clip = preColor - p_clip;
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    float factor = rcp(max(1.0, ma_unit));
    return p_clip + v_clip * factor;
}
float3 TAAUVarianceClipBox(float3 m1, float3 m2, float gamma, float3 preColor)
{
    float3 mu = m1;
    float3 sigma = sqrt(abs(m2 - mu * mu)) + FLT_EPS;
    float3 colorMin = mu - gamma * sigma;
    float3 colorMax = mu + gamma * sigma;

    colorMin.x = max(colorMin.x, 0.0f);
    colorMax.x = max(colorMax.x, 0.0f);

    float3 p_clip = 0.5 * (colorMax + colorMin);
    float3 e_clip = 0.5 * (colorMax - colorMin) + FLT_EPS;

    float3 v_clip = preColor - p_clip;
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    float factor = rcp(max(1.0, ma_unit));
    return p_clip + v_clip * factor;
}

float CalcTAAWeight(float staticWeight, float dynamicWeight, float maxWeight, float velocityFactor)
{
    return lerp(staticWeight, maxWeight, saturate(dynamicWeight * velocityFactor));
}

float3 CatmullRomSample(UINT texIndex, float2 uv, float2 duv)
{
    float2 samplePos = uv / duv;
    float2 f = frac(samplePos - 0.5);
    float2 i = floor(samplePos - 0.5);

    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);

    float2 w12 = w1 + w2;
    float2 offset = w2 / w12;

    float2 uv0 = (i - 0.5) * duv;
    float2 uv3 = (i + 2.5) * duv;
    float2 uv12 = (i + 0.5 + offset) * duv;

    float3 color = 0;
    color += SampleTexture2D(texIndex, float2(uv12.x, uv0.y), ClampLinearSampler) * (w12.x * w0.y);
    color += SampleTexture2D(texIndex, float2(uv0.x, uv12.y), ClampLinearSampler) * (w0.x * w12.y);
    color += SampleTexture2D(texIndex, float2(uv12.x, uv12.y), ClampLinearSampler) * (w12.x * w12.y);
    color += SampleTexture2D(texIndex, float2(uv3.x, uv12.y), ClampLinearSampler) * (w3.x * w12.y);
    color += SampleTexture2D(texIndex, float2(uv12.x, uv3.y), ClampLinearSampler) * (w12.x * w3.y);

    return max(0, color);
}

//
// ------------------------------------------------------------------- TAAU -------------------------------------------------------------------
// 
float ComputeTAAUWeight(float2 pixelDelta, float upscaleFactor)
{
    float u2 = upscaleFactor * upscaleFactor;
    // 高分辨率下距离的平方
    float x2 = saturate(u2 * dot(pixelDelta, pixelDelta));
    // 拟合曲线:1 - 1.9 * x^2 + 0.9 * x^4
    return max(0.0f, (0.905f * x2 - 1.9f) * x2 + 1.0f);
}
void DownSample3x3(UINT currFrameTexIndex,
                   float2 downCenterPos,
                   float4 downTexSize,
                   float2 posCenterToJitter,
                   float upScaleFactor,
                   out float3 currColor,
                   out float3 minColor,
                   out float3 maxColor,
                   out float3 m1,
                   out float3 m2,
                   out float2 cloestUV)
{
    currColor = 0.f;
    minColor = FLT_MAX;
    maxColor = FLT_MIN;
    cloestUV = 0.f;
    m1 = 0.f;
    m2 = 0.f;

    float du = downTexSize.z;
    float dv = downTexSize.w;
    const float2 offsetUV[9] =
    {
        {-du, dv}, {0, dv}, {du, dv},
        {-du, 0}, {0, 0}, {du, 0},
        {-du, -dv}, {0, -dv}, {du, -dv}
    };
    float distThresholdSq = lerp(1.51f, 1.3f, upScaleFactor - 1.0f);
    distThresholdSq *= distThresholdSq;
    float validVarianceSamples = FLT_EPS;

    float totalWeight = FLT_EPS;
    float minDepth = FLT_MAX;
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 samplePos = downCenterPos + offsetUV[i] * downTexSize.xy;
        float2 sampleUV = samplePos * downTexSize.zw;

        float depth = SampleTexture2D(OpaqueDepthIndex, sampleUV, ClampPointSampler);
        [branch]
        if (minDepth > depth)
        {
            minDepth = depth;
            cloestUV = sampleUV;
        }

        float3 color = SampleTexture2D(currFrameTexIndex, sampleUV, ClampPointSampler);
        if (any(isnan(color)) || any(isinf(color)))
        {
            color = float3(0.0f, 0.0f, 0.0f);
        }
        color = clamp(color, 0.0f, 65504.0f);
        float luma = max(color.r, max(color.g, color.b));
        float karisWeight = 1.0f / (1.0f + luma);
        color = TransformRGB2YCoCg(ReinhardTonemap(color));

        minColor = min(minColor, color);
        maxColor = max(maxColor, color);

        float2 posSampleToJitter = offsetUV[i] * downTexSize.xy - posCenterToJitter;

        float spatialWeigh = ComputeTAAUWeight(posSampleToJitter, upScaleFactor);
        spatialWeigh *= karisWeight;
        currColor += color * spatialWeigh;
        totalWeight += spatialWeigh;

        if (dot(posSampleToJitter, posSampleToJitter) < distThresholdSq)
        {
            m1 += color;
            m2 += color * color;
            validVarianceSamples += 1.f;
        }
    }

    currColor *= rcp(totalWeight);
    m1 *= rcp(validVarianceSamples);
    m2 *= rcp(validVarianceSamples);
}

float KarisWeight(float3 color)
{
    float luma = max(color.r, max(color.g, color.b));
    return 1.0f / (1.0f + luma);
}
#endif