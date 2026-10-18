#include "private\Common.hlsl"
#include "private\Color.hlsl"
#include "private\AutoExposureCommon.hlsli"

#define AE_VIS_SCENE 0
#define AE_VIS_HISTOGRAM_DEBUG 1
#define AE_VIS_LUMINANCE 2

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_DestTexIndex;
    UINT g_HdrTexIndex;
    UINT g_HistogramTexIndex;
    UINT g_ExposureScaleBufferIndex;

    UINT g_VisualizeDebugType;
    float g_LuminanceScale;
    float g_HistogramScale;
    float g_HistogramBias;

    float g_LowPercent;
    float g_HighPercent;
    float g_MinAverageLuminance;
    float g_MaxAverageLuminance;

    float g_MiddleGrey;
    Vector4 g_DestSize; // xy = size, zw = inv size
}

groupshared float gsHist[HISTOGRAM_SIZE];
groupshared float gsHistMax;
groupshared float gsHistSum;

uint2 AEVisGlyph(uint code)
{
    // 5x7, 8 bits per row. code: 0-9 digits, 10='-', 11='.'
    switch (code)
    {
    case 0:  return uint2(0x1111110E, 0x000E1111);
    case 1:  return uint2(0x04040C04, 0x000E0404);
    case 2:  return uint2(0x0601110E, 0x001F1008);
    case 3:  return uint2(0x0601110E, 0x000E1101);
    case 4:  return uint2(0x120A0602, 0x0002021F);
    case 5:  return uint2(0x011E101F, 0x000E1101);
    case 6:  return uint2(0x1E100806, 0x000E1111);
    case 7:  return uint2(0x0402011F, 0x00080808);
    case 8:  return uint2(0x0E11110E, 0x000E1111);
    case 9:  return uint2(0x0F11110E, 0x000C0201);
    case 10: return uint2(0x1F000000, 0x00000000);
    case 11: return uint2(0x00000000, 0x000C0C00);
    default: return uint2(0, 0);
    }
}

bool AEVisGlyphPixel(uint2 glyph, int x, int y)
{
    if (x < 0 || x > 4 || y < 0 || y > 6)
    {
        return false;
    }
    uint packed = (y < 4) ? glyph.x : glyph.y;
    uint row = (packed >> (8u * (uint)(y & 3))) & 0xFFu;
    // Packed as 5-bit rows with bit 4 = leftmost pixel. Using bit x mirrored 2/5/3/7.
    return ((row >> (4 - x)) & 1u) != 0u;
}

void AEVisPlotGlyph(int2 pixel, inout float3 color, float3 fontColor, int2 topLeft, uint code)
{
    int2 rel = pixel - topLeft;
    if (AEVisGlyphPixel(AEVisGlyph(code), rel.x, rel.y))
    {
        color = fontColor;
    }
}

uint AEVisDigit(float number, float place)
{
    float scaled = number / place;
    return (uint)floor(fmod(scaled, 10.0f));
}

void AEVisPrintSmallFloat(int2 pixel, inout float3 color, float3 fontColor, int2 leftTop, float number)
{
    int2 cursor = leftTop;
    bool bNeg = number < 0.0f;
    number = abs(number) + 0.005f;
    bool bGe10 = number >= 10.0f;

    if (bNeg)
    {
        AEVisPlotGlyph(pixel, color, fontColor, cursor, 10);
        cursor.x += 6;
    }
    else
    {
        cursor.x += 6;
    }

    if (bGe10)
    {
        AEVisPlotGlyph(pixel, color, fontColor, cursor, AEVisDigit(number, 10.0f));
        cursor.x += 6;
    }
    else
    {
        cursor.x += 6;
    }

    AEVisPlotGlyph(pixel, color, fontColor, cursor, AEVisDigit(number, 1.0f));
    cursor.x += 6;
    AEVisPlotGlyph(pixel, color, fontColor, cursor, 11);
    cursor.x += 6;
    AEVisPlotGlyph(pixel, color, fontColor, cursor, AEVisDigit(number, 0.1f));
    cursor.x += 6;
    AEVisPlotGlyph(pixel, color, fontColor, cursor, AEVisDigit(number, 0.01f));
}

void AEVisPrintFloat(int2 pixel, inout float3 color, float3 fontColor, int2 leftTop, float number)
{
    int2 cursor = leftTop;
    if (number >= 1000.0f)
    {
        number = min(number, 999999.0f);
        uint value = (uint)number;
        uint places[6] = { 100000, 10000, 1000, 100, 10, 1 };
        bool started = false;
        [unroll]
        for (uint i = 0; i < 6; ++i)
        {
            uint d = (value / places[i]) % 10;
            if (d != 0 || started || i == 5)
            {
                started = true;
                AEVisPlotGlyph(pixel, color, fontColor, cursor, d);
                cursor.x += 6;
            }
        }
        return;
    }
    AEVisPrintSmallFloat(pixel, color, fontColor, leftTop, number);
}

float AEVisPlaceInHistogram(float luminance)
{
    float logL = log2(max(luminance, 1e-5f));
    float sumTotal = 0.0f;
    float sumBelow = 0.0f;
    [unroll]
    for (uint i = 0; i < HISTOGRAM_SIZE; ++i)
    {
        float weight = gsHist[i];
        float logLo = ComputeLogLuminanceFromHistogramPosition((float)i / (float)(HISTOGRAM_SIZE - 1),
                                                               g_HistogramScale, g_HistogramBias);
        float logHi = ComputeLogLuminanceFromHistogramPosition((float)(i + 1) / (float)(HISTOGRAM_SIZE - 1),
                                                               g_HistogramScale, g_HistogramBias);
        float t = saturate((logL - logLo) / max(logHi - logLo, 1e-6f));
        sumBelow += weight * t;
        sumTotal += weight;
    }
    return sumBelow / max(sumTotal, 1e-4f);
}

[numthreads(8, 8, 1)]
void VisualizeHDR(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    Texture2D<UINT> histogramTex = ResourceDescriptorHeap[g_HistogramTexIndex];
    if (groupIndex < HISTOGRAM_SIZE)
    {
        gsHist[groupIndex] = histogramTex.Load(int3(groupIndex, 0, 0));
    }
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        float histMax = 0.0f;
        float histSum = 0.0f;
        [unroll]
        for (uint i = 0; i < HISTOGRAM_SIZE; ++i)
        {
            histMax = max(histMax, gsHist[i]);
            histSum += gsHist[i];
        }
        gsHistMax = max(histMax, 1e-4f);
        gsHistSum = max(histSum, 1e-4f);
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 destSize = (uint2)g_DestSize.xy;
    if (dispatchThreadID.x >= destSize.x || dispatchThreadID.y >= destSize.y)
    {
        return;
    }

    int2 pixel = (int2)dispatchThreadID.xy;
    RWTexture2D<float4> dest = ResourceDescriptorHeap[g_DestTexIndex];
    StructuredBuffer<float4> exposureTex = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];

    float3 sceneColor = dest[pixel].rgb;
    float3 hdrColor = LoadTexture2D(g_HdrTexIndex, pixel).rgb;
    float4 eye = exposureTex[0];

    float exposureScale = max(eye.r, 1e-6f);
    float scaleWithoutBias = max(eye.g, 1e-6f);
    float avgLum = eye.b;
    float middleGrey = max(g_MiddleGrey, 1e-4f);

    float3 outColor = sceneColor;
    if (g_VisualizeDebugType == AE_VIS_HISTOGRAM_DEBUG)
    {
        float percentile = AEVisPlaceInHistogram(AutoExposureLuminance(hdrColor));
        outColor = percentile.xxx;
        if (percentile < saturate(g_LowPercent))
        {
            outColor = float3(0.5f, 0.0f, 0.0f);
        }
        else if (percentile >= saturate(max(g_HighPercent, g_LowPercent)))
        {
            outColor = float3(0.0f, 0.0f, 0.5f);
        }
    }
    else if (g_VisualizeDebugType == AE_VIS_LUMINANCE)
    {
        outColor = AutoExposureLuminance(hdrColor) * g_LuminanceScale;
    }

    const int2 histogramLeftTop = int2((int)g_DestSize.x > AE_VIS_HISTOGRAM_MARGIN_X * 2
                                           ? AE_VIS_HISTOGRAM_MARGIN_X
                                           : 8,
                                       (int)g_DestSize.y - AE_VIS_HISTOGRAM_HEIGHT - AE_VIS_HISTOGRAM_MARGIN_BOTTOM);
    const int2 histogramSize = int2((int)g_DestSize.x - histogramLeftTop.x * 2, AE_VIS_HISTOGRAM_HEIGHT);
    const int histogramOuterBorder = AE_VIS_HISTOGRAM_OUTER_BORDER;

    if (histogramSize.x > 8 && histogramSize.y > 8)
    {
        float2 insetPx = float2(pixel - histogramLeftTop);
        float2 insetUV = insetPx / float2(histogramSize);
        float3 borderColor = AutoExposureColorize(insetUV.x);
        float borderDistance = AutoExposureDistanceToRect(pixel, histogramLeftTop, histogramSize);

        outColor = lerp(float3(0, 0, 0), outColor, saturate(borderDistance - (histogramOuterBorder + 2)));
        outColor = lerp(borderColor, outColor, saturate(borderDistance - (histogramOuterBorder + 1)));
        outColor = lerp(float3(0, 0, 0), outColor, saturate(borderDistance - 1));

        if (borderDistance <= 0.0f)
        {
            uint bucket = min((uint)(saturate(insetUV.x) * (float)HISTOGRAM_SIZE), (uint)(HISTOGRAM_SIZE - 1));

            float minExposure = g_MinAverageLuminance / middleGrey;
            float maxExposure = g_MaxAverageLuminance / middleGrey;
            float minPos = ComputeHistogramPositionFromLuminance(minExposure, g_HistogramScale, g_HistogramBias);
            float maxPos = ComputeHistogramPositionFromLuminance(maxExposure, g_HistogramScale, g_HistogramBias);

            if (insetUV.x < minPos || insetUV.x >= maxPos)
            {
                outColor = lerp(outColor, float3(0.5f, 0.5f, 0.5f), 0.5f);
            }
            else
            {
                outColor = lerp(outColor, float3(0.5f, 0.8f, 0.5f), 0.5f);
            }

            float localHist = gsHist[bucket] / gsHistMax;
            if (localHist >= (1.0f - insetUV.y))
            {
                outColor = lerp(outColor, AutoExposureColorize(insetUV.x), 0.5f);
            }

            float lumAtX = ComputeLuminanceFromHistogramPosition(insetUV.x, g_HistogramScale, g_HistogramBias);
            float3 adapted = exposureScale * lumAtX;
            float3 tone = NeutralTonemap(adapted);
            float3 distMask = saturate(1.0f - 100.0f * abs(tone - (1.0f - insetUV.y)));
            outColor = lerp(outColor, float3(1, 1, 1), distMask);

            float bluePos = ComputeHistogramPositionFromLuminance(avgLum / middleGrey, g_HistogramScale, g_HistogramBias);
            if (abs(insetPx.x - bluePos * histogramSize.x) < 3.0f)
            {
                outColor = lerp(outColor, float3(0, 0, 1), 0.5f);
            }

            float purpleLum = 1.0f / scaleWithoutBias;
            float purplePos = ComputeHistogramPositionFromLuminance(purpleLum, g_HistogramScale, g_HistogramBias);
            float purplePx = purplePos * histogramSize.x;
            AEVisPrintSmallFloat(pixel, outColor, float3(1.0f, 0.0f, 1.0f),
                                 histogramLeftTop + int2((int)purplePx - 6 * 8 - 3, 1),
                                 log2(max(purpleLum, 1e-5f)));
            if (abs(insetPx.x - purplePx) < 2.0f && pixel.y > histogramLeftTop.y + 9)
            {
                outColor = float3(0.5f, 0.0f, 0.5f);
            }

            float whiteLum = 1.0f / exposureScale;
            float whitePos = ComputeHistogramPositionFromLuminance(whiteLum, g_HistogramScale, g_HistogramBias);
            float whitePx = whitePos * histogramSize.x;
            AEVisPrintSmallFloat(pixel, outColor, float3(1, 1, 1),
                                 histogramLeftTop + int2((int)whitePx - 6 * 8 - 3, 1),
                                 log2(max(whiteLum, 1e-5f)));
            if (abs(insetPx.x - whitePx) < 2.0f && pixel.y > histogramLeftTop.y + 18)
            {
                outColor = float3(1, 1, 1);
            }
        }
    }

    int2 center = (int2)(destSize / 2);
    int dx = abs(pixel.x - center.x);
    int dy = abs(pixel.y - center.y);
    const int meterHudW = 180;
    const int meterHudH = 42;
    const int meterHalf = AE_VIS_METER_HALF_SIZE;
    if (dx < meterHudW && dy < meterHudH)
    {
        float3 hdrAvg = 0.0f;
        float sampleCount = 0.0f;
        [loop]
        for (int oy = -meterHalf; oy <= meterHalf; ++oy)
        {
            [loop]
            for (int ox = -meterHalf; ox <= meterHalf; ++ox)
            {
                int2 samplePx = center + int2(ox, oy);
                samplePx = clamp(samplePx, int2(0, 0), int2(destSize) - 1);
                hdrAvg += LoadTexture2D(g_HdrTexIndex, samplePx).rgb;
                sampleCount += 1.0f;
            }
        }
        hdrAvg /= max(sampleCount, 1.0f);

        if (dx < meterHalf && dy < meterHalf)
        {
            if (dx == meterHalf - 1 || dy == meterHalf - 1)
            {
                outColor = float3(1, 1, 1);
            }
            else
            {
                outColor = sceneColor;
            }
        }

        float lum = AutoExposureLuminance(hdrAvg);
        const float3 numberColor = float3(0.8f, 0.85f, 0.85f);
        AEVisPrintFloat(pixel, outColor, numberColor, center + int2(11, 11), lum);
        float ev100 = log2(max(lum / middleGrey, 1e-5f));
        AEVisPrintSmallFloat(pixel, outColor, numberColor, center + int2(11, 22), ev100);
    }

    dest[pixel] = float4(outColor, 1.0f);
}
