#include "private\Common.hlsl"
#include "private\AutoExposureCommon.hlsli"

#ifndef EXPOSURE_COMPENSATION_CURVE_SAMPLES
#define EXPOSURE_COMPENSATION_CURVE_SAMPLES 64
#endif

#define CLEAR_HISTOGRAM_THREAD_GROUP [numthreads(HISTOGRAM_SIZE, 1, 1)]
#define BUILD_HISTOGRAM_THREAD_GROUP [numthreads(BUILD_HISTOGRAM_THREAD_GROUP_SIZE, BUILD_HISTOGRAM_THREAD_GROUP_SIZE, 1)]
#define COMPUTE_EXPOSURE_THREAD_GROUP [numthreads(HISTOGRAM_SIZE, 1, 1)]
#define RESET_PRE_EXPOSURE_THREAD_GROUP [numthreads(1, 1, 1)]

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SourceTexIndex;
    UINT g_HalfResSourceTexIndex;
    UINT g_HistogramBufferIndex;
    UINT g_ExposureScaleBufferIndex;
    UINT g_PreExposureScaleBufferIndex;
    UINT g_HistogramTexIndex;
    Vector4 g_SourceSize;
    Vector4 g_HalfSize;

    float g_HistogramScale;
    float g_HistogramBias;
    float g_LowPercent;
    float g_HighPercent;

    float g_MinAverageLuminance;
    float g_MaxAverageLuminance;
    float g_MiddleGrey;
    float g_ExposureCompensationEV;
    float g_ExposureSpeedUp;
    float g_ExposureSpeedDown;
    float g_ExponentialTransitionDistance;
    float g_DeltaTime;
    UINT g_PreExposureEnable;
    UINT g_WriteHistogramTex;
    float g_ForceTarget;

    // UE ExposureCompensationCurveLUT, packed as float4 because HLSL float[]
    // has a 16-byte stride and the pass uploader writes tightly packed floats.
    // Each lane is exp2(bias). Scale/Bias map scene EV100 onto texel centers.
    float g_ExposureCompensationCurveScale;
    float g_ExposureCompensationCurveBias;
    float4 g_ExposureCompensationCurve[EXPOSURE_COMPENSATION_CURVE_SAMPLES / 4];
}

// --------------------------------------------------------------------------------------- Clear Histogram ---------------------------------------------------------------------------------------
CLEAR_HISTOGRAM_THREAD_GROUP
void ClearHistogram(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> histogram = ResourceDescriptorHeap[g_HistogramBufferIndex];
    histogram[dispatchThreadID.x] = 0;
}

// --------------------------------------------------------------------------------------- Build Histogram ---------------------------------------------------------------------------------------

groupshared uint GSLocalHistogram[HISTOGRAM_SIZE];

float GetExposureScaleWithoutEV(RWStructuredBuffer<float4> ExposureScaleBuffer)
{
    return ExposureScaleBuffer[0].g;
}

float GetOneOverPreExposureScale()
{
    if (g_PreExposureEnable == 0)
        return 1.0f;
    RWStructuredBuffer<float2> PreExposureScaleBuffer = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    return rcp(PreExposureScaleBuffer[0].g);
}

BUILD_HISTOGRAM_THREAD_GROUP
void BuildHistogram(uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    RWStructuredBuffer<uint> HistogramBuffer = ResourceDescriptorHeap[g_HistogramBufferIndex];
    uint2 SamplePos = DispatchThreadID.xy;

    [branch]
    if (GroupIndex < HISTOGRAM_SIZE)
    {
        GSLocalHistogram[GroupIndex] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 SourceSize = (uint2)g_HalfSize.xy;
    if (SamplePos.x < SourceSize.x && SamplePos.y < SourceSize.y)
    {
        float OneOverPreExposureScale = GetOneOverPreExposureScale();
        float3 SourceColor = LoadTexture2D(g_HalfResSourceTexIndex, SamplePos) * OneOverPreExposureScale;
        float Luminance = AutoExposureLuminance(SourceColor);
        uint BinIndex = LuminanceToHistogramBin(Luminance, g_HistogramScale, g_HistogramBias);
        InterlockedAdd(GSLocalHistogram[BinIndex], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    [branch]
    if (GroupIndex < HISTOGRAM_SIZE)
    {
        InterlockedAdd(HistogramBuffer[GroupIndex], GSLocalHistogram[GroupIndex]);
    }
}


// --------------------------------------------------------------------------------------- Compute Exposure ---------------------------------------------------------------------------------------
groupshared uint GSGlobalHistogram[HISTOGRAM_SIZE];

// r = final scale, g = scale without constant EV, b = avg lum, a = middleGrey * compensation
void UpdateExposureScaleBuffer(RWStructuredBuffer<float4> ExposureScaleBuffer,
                               float ExposureScale,
                               float ExposureScaleWithoutEV,
                               float AvgLuminance,
                               float MiddleGreyLuminance)
{
    ExposureScaleBuffer[0] = float4(ExposureScale, ExposureScaleWithoutEV, AvgLuminance, MiddleGreyLuminance);
}

void UpdateHistoryPreExposureScaleBuffer(RWStructuredBuffer<float2> PreExposureScaleBuffer)
{
    PreExposureScaleBuffer[0].r = PreExposureScaleBuffer[0].g;
}

void UpdateCurrPreExposureScaleBuffer(RWStructuredBuffer<float2> PreExposureScaleBuffer, float NewExposureScale)
{
    PreExposureScaleBuffer[0].g = NewExposureScale;
}

void ApplyPreExposureUpdate(RWStructuredBuffer<float2> PreExposureScaleBuffer, float newScale)
{
    if (g_PreExposureEnable == 0)
        return;
    UpdateHistoryPreExposureScaleBuffer(PreExposureScaleBuffer);
    UpdateCurrPreExposureScaleBuffer(PreExposureScaleBuffer, newScale);
}

void ApplyDefaultForExposure(RWStructuredBuffer<float2> PreExposureScaleBuffer,
                             RWStructuredBuffer<float4> ExposureScaleBuffer,
                             float MiddleGreyLuminance)
{
    ExposureScaleBuffer[0] = float4(1.f, 1.f, 0.f, MiddleGreyLuminance);
    ApplyPreExposureUpdate(PreExposureScaleBuffer, 1.f);
}

bool ComputeExposureSetup(UINT GroupIndex,
                          RWTexture2D<UINT> HistogramRT,
                          RWStructuredBuffer<uint> HistogramBuffer)
{
    if (GroupIndex < HISTOGRAM_SIZE)
    {
        GSGlobalHistogram[GroupIndex] = HistogramBuffer[GroupIndex];
        // Debug viz only: skip UAV write when visualizeHDR is off.
        if (g_WriteHistogramTex != 0)
        {
            HistogramRT[UINT2(GroupIndex, 0)] = GSGlobalHistogram[GroupIndex];
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (GroupIndex > 0)
    {
        return false;
    }

    return true;
}

float ComputeHistogramSum()
{
    uint SumBin = 0;
    UNROLL
    for (UINT i = 0; i < HISTOGRAM_SIZE; i++)
    {
        SumBin += GSGlobalHistogram[i];
    }

    return SumBin;
}

float ComputeAvgLuminance(UINT SumBin)
{
    float MinBin = (float)SumBin * saturate(g_LowPercent);
    float MaxBin = (float)SumBin * saturate(max(g_HighPercent, g_LowPercent));

    float TotalEv = 0;
    float TotalCount = 0;
    [unroll]
    for (UINT i = 0; i < HISTOGRAM_SIZE; i++)
    {
        float CurrBin = GSGlobalHistogram[i];
        float CurrSub = min(CurrBin, MinBin);

        CurrBin -= CurrSub;
        MinBin -= CurrSub;
        MaxBin -= CurrSub;

        CurrBin = min(CurrBin, MaxBin);
        MaxBin -= CurrBin;

        float HistogramPos = float(i) / (float(HISTOGRAM_SIZE) - 1);
        TotalEv += ComputeLogLuminanceFromHistogramPosition(HistogramPos, g_HistogramScale, g_HistogramBias) * CurrBin;
        TotalCount += CurrBin;
    }

    float AvgEV = TotalEv * rcp(max(TotalCount, 1e-5));
    float AvgLuminance = ComputeLuminanceFromLogLuminance(AvgEV);

    return AvgLuminance;
}

float ExposureCompensationCurveAt(uint index)
{
    float4 lane = g_ExposureCompensationCurve[index >> 2];
    uint channel = index & 3;
    if (channel == 0)
        return lane.x;
    if (channel == 1)
        return lane.y;
    if (channel == 2)
        return lane.z;
    return lane.w;
}

// UE EyeAdaptationCommon curve sample. X is log2(average / middleGrey): with
// LuminanceMax = 1 that is the saturation EV100 of this frame's meter.
float SampleExposureCompensationCurve(float averageLuminance, float middleGrey)
{
    float ev100 = log2(max(averageLuminance, 1e-8f) / max(middleGrey, 1e-5f));
    float u = ev100 * g_ExposureCompensationCurveScale + g_ExposureCompensationCurveBias;
    float coord = clamp(u * EXPOSURE_COMPENSATION_CURVE_SAMPLES - 0.5f,
                        0.0f,
                        EXPOSURE_COMPENSATION_CURVE_SAMPLES - 1.0f);
    uint i0 = (uint)coord;
    uint i1 = min(i0 + 1u, (uint)EXPOSURE_COMPENSATION_CURVE_SAMPLES - 1u);
    float t = coord - float(i0);
    return lerp(ExposureCompensationCurveAt(i0), ExposureCompensationCurveAt(i1), t);
}

// UE EyeAdaptationCommon: adapt scene brightness relative to middle grey (avg / middleGrey)
// in log2 (f-stop) space, then invert back to multiply scale.
// The curve scale is divided out of history, so a curve change eases instead of popping.
// Constant Exposure Compensation stays outside that history and applies immediately.
// .r = final scale, .g = scale without constant EV, .b = avg lum, .a = middleGrey * compensation
float4 ComputeTemporalSmooth(float AvgLuminance, float OldExposureScaleWithoutEV)
{
    float MiddleGrey = max(g_MiddleGrey, 1e-5);
    float TargetAvg = clamp(AvgLuminance, g_MinAverageLuminance, g_MaxAverageLuminance);
    float TargetExposure = TargetAvg / MiddleGrey;
    float CurveScale = SampleExposureCompensationCurve(AvgLuminance, MiddleGrey);
    float OldExposure = CurveScale * rcp(max(OldExposureScaleWithoutEV, 1e-5));

    float ForceTarget = g_ForceTarget;
    if (OldExposureScaleWithoutEV <= 1e-5)
    {
        ForceTarget = 1.0f;
    }

    float SmoothedExposure = AutoExposureAdapt(OldExposure,
                                               TargetExposure,
                                               g_DeltaTime,
                                               g_ExposureSpeedUp,
                                               g_ExposureSpeedDown,
                                               g_ExponentialTransitionDistance,
                                               ForceTarget);
    SmoothedExposure = clamp(SmoothedExposure,
                             g_MinAverageLuminance / MiddleGrey,
                             g_MaxAverageLuminance / MiddleGrey);

    float ExposureScaleWithoutEV = CurveScale * rcp(max(SmoothedExposure, 1e-5));
    float EC = ComputeLuminanceFromLogLuminance(g_ExposureCompensationEV);
    float ExposureScale = ExposureScaleWithoutEV * EC;
    float MiddleGreyLuminance = MiddleGrey * EC * CurveScale;

    return float4(ExposureScale, ExposureScaleWithoutEV, AvgLuminance, MiddleGreyLuminance);
}


RESET_PRE_EXPOSURE_THREAD_GROUP
void ResetPreExposure()
{
    RWStructuredBuffer<float2> PreExposureScaleBuffer = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    PreExposureScaleBuffer[0] = float2(1.0f, 1.0f);
}

RESET_PRE_EXPOSURE_THREAD_GROUP
void InitExposureState()
{
    RWStructuredBuffer<float4> ExposureScaleBuffer = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
    RWStructuredBuffer<float2> PreExposureScaleBuffer = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    ExposureScaleBuffer[0] = float4(1.0f, 1.0f, 0.0f, max(g_MiddleGrey, 0.18f));
    PreExposureScaleBuffer[0] = float2(1.0f, 1.0f);
}

COMPUTE_EXPOSURE_THREAD_GROUP
void ComputeExposure(uint GroupIndex : SV_GroupIndex)
{
    RWStructuredBuffer<uint> HistogramBuffer = ResourceDescriptorHeap[g_HistogramBufferIndex];
    RWTexture2D<UINT> HistogramRT = ResourceDescriptorHeap[g_HistogramTexIndex];
    RWStructuredBuffer<float4> ExposureScaleBuffer = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
    RWStructuredBuffer<float2> PreExposureScaleBuffer = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];

    BRANCH
    if (!ComputeExposureSetup(GroupIndex, HistogramRT, HistogramBuffer))
    {
        return;
    }

    UINT SumBin = ComputeHistogramSum();
    float MiddleGreyLuminance = g_MiddleGrey * ComputeLuminanceFromLogLuminance(g_ExposureCompensationEV);

    BRANCH
    if (SumBin < 1.f)
    {
        ApplyDefaultForExposure(PreExposureScaleBuffer, ExposureScaleBuffer, MiddleGreyLuminance);
        return;
    }

    float AvgLuminance = ComputeAvgLuminance(SumBin);
    float OldExposureScaleWithoutEV = GetExposureScaleWithoutEV(ExposureScaleBuffer);
    float4 ExposureData = ComputeTemporalSmooth(AvgLuminance, OldExposureScaleWithoutEV);

    UpdateExposureScaleBuffer(ExposureScaleBuffer,
                              ExposureData.r,
                              ExposureData.g,
                              ExposureData.b,
                              ExposureData.a);
    ApplyPreExposureUpdate(PreExposureScaleBuffer, ExposureData.r);
}