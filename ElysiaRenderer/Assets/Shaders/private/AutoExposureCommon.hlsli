#ifndef AUTO_EXPOSURE_COMMON_HLSLI
#define AUTO_EXPOSURE_COMMON_HLSLI

#ifndef HISTOGRAM_SIZE
#define HISTOGRAM_SIZE 64
#endif

// Keep in sync with AutoExposureVisualize in AutoExposureUtility.h
#define AE_VIS_HISTOGRAM_MARGIN_X 64
#define AE_VIS_HISTOGRAM_HEIGHT 128
#define AE_VIS_HISTOGRAM_MARGIN_BOTTOM 32
#define AE_VIS_HISTOGRAM_OUTER_BORDER 4
#define AE_VIS_METER_HALF_SIZE 10

// Rec.709 luminance
float AutoExposureLuminance(float3 color)
{
    return max(dot(color, float3(0.2126f, 0.7152f, 0.0722f)), 0.0f);
}

float ComputeHistogramBinFromLuminance(float logLuminance, float histogramScale, float histogramBias)
{
    return logLuminance * histogramScale + histogramBias;
}

float ComputeLogLuminanceFromHistogramPosition(float histogramPosition, float histogramScale, float histogramBias)
{
    return (histogramPosition - histogramBias) / max(histogramScale, 1e-6f);
}

float ComputeHistogramPositionFromLuminance(float luminance, float histogramScale, float histogramBias)
{
    return ComputeHistogramBinFromLuminance(log2(max(luminance, 1e-5f)), histogramScale, histogramBias);
}

float ComputeLuminanceFromHistogramPosition(float histogramPosition, float histogramScale, float histogramBias)
{
    return exp2(ComputeLogLuminanceFromHistogramPosition(histogramPosition, histogramScale, histogramBias));
}

float ComputeLuminanceFromLogLuminance(float LogLuminance)
{
    return exp2(LogLuminance);
}

uint LuminanceToHistogramBin(float Luminance, float EVScale, float EVBias)
{
    float EV = log2(max(1e-5, Luminance));
    float BinPos = saturate(ComputeHistogramBinFromLuminance(EV, EVScale, EVBias));
    return (uint)(BinPos * (HISTOGRAM_SIZE - 1));
}

// UE PostProcessVisualizeHDR.usf Colorize: 0=cold (blue) .. 0.5=green .. 1=hot (red).
float3 AutoExposureColorize(float x)
{
    x = saturate(x);
    float3 heat = float3(1.0f, 0.0f, 0.0f);
    float3 middle = float3(0.0f, 1.0f, 0.0f);
    float3 cold = float3(0.0f, 0.0f, 1.0f);
    float3 coldHeat = lerp(cold, heat, x);
    return lerp(middle, coldHeat, abs(0.5f - x) * 2.0f);
}

float AutoExposureDistanceToRect(int2 pixel, int2 leftTop, int2 size)
{
    int2 bmax = leftTop + size - 1;
    float dx = max(float(leftTop.x - pixel.x), float(pixel.x - bmax.x));
    float dy = max(float(leftTop.y - pixel.y), float(pixel.y - bmax.y));
    return max(dx, dy);
}

#endif
