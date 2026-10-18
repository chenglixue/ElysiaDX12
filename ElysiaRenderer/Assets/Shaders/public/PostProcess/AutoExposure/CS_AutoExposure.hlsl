#include "private\Common.hlsl"
#include "private\AutoExposureCommon.hlsli"

#define CLEAR_HISTOGRAM_THREAD_GROUP [numthreads(HISTOGRAM_SIZE, 1, 1)]
#define BUILD_HISTOGRAM_THREAD_GROUP [numthreads(BUILD_HISTOGRAM_THREAD_GROUP_SIZE, BUILD_HISTOGRAM_THREAD_GROUP_SIZE, 1)]
#define COMPUTE_EXPOSURE_THREAD_GROUP [numthreads(HISTOGRAM_SIZE, 1, 1)]

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SourceTexIndex;
    UINT g_HistogramBufferIndex;
    UINT g_ExposureScaleBufferIndex;
    UINT g_HistogramTexIndex;
    Vector4 g_SourceSize;

    float g_HistogramScale;
    float g_HistogramBias;
    float g_LowPercent;
    float g_HighPercent;

    float g_MinAverageLuminance;
    float g_MaxAverageLuminance;
    float g_MiddleGrey;
    float g_ExposureCompensationEV;
}

CLEAR_HISTOGRAM_THREAD_GROUP
void ClearHistogram(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> histogram = ResourceDescriptorHeap[g_HistogramBufferIndex];
    histogram[dispatchThreadID.x] = 0;
}

groupshared uint GSLocalHistogram[HISTOGRAM_SIZE];

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
    
    uint2 SourceSize = (uint2)g_SourceSize.xy;
    if (SamplePos.x < SourceSize.x && SamplePos.y < SourceSize.y)
    {
        float3 SourceColor = LoadTexture2D(g_SourceTexIndex, SamplePos);
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

    // uint2 SourceSize = (uint2)g_SourceSize.xy;
    // if (dispatchThreadID.x < SourceSize.x && dispatchThreadID.y < SourceSize.y)
    // {
    //     float3 SourceColor = LoadTexture2D(g_SourceTexIndex, dispatchThreadID.xy).rgb;
    //     float Luminance = AutoExposureLuminance(SourceColor);
    //     uint bin = LuminanceToHistogramBin(Luminance, g_HistogramScale, g_HistogramBias);
    //     InterlockedAdd(gsLocalHistogram[bin], 1);
    // }
    //
    // GroupMemoryBarrierWithGroupSync();
    //
    // if (groupIndex < HISTOGRAM_SIZE)
    // {
    //     InterlockedAdd(histogram[groupIndex], gsLocalHistogram[groupIndex]);
    // }
}

groupshared uint GSGlobalHistogram[HISTOGRAM_SIZE];

COMPUTE_EXPOSURE_THREAD_GROUP
void ComputeExposure(uint groupIndex : SV_GroupIndex)
{
    RWStructuredBuffer<uint> HistogramBuffer = ResourceDescriptorHeap[g_HistogramBufferIndex];
    RWTexture2D<UINT> HistogramRT = ResourceDescriptorHeap[g_HistogramTexIndex];
    RWStructuredBuffer<float4> ExposureScaleBuffer = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
    
    if (groupIndex < HISTOGRAM_SIZE)
    {
        HistogramRT[UINT2(groupIndex, 0)] = GSGlobalHistogram[groupIndex] = HistogramBuffer[groupIndex];
    }
    GroupMemoryBarrierWithGroupSync();
    
    if (groupIndex > 0)
    {
        return;
    }
    
    uint SumBin = 0;
    [unroll]
    for (UINT i = 0;i < HISTOGRAM_SIZE; i++)
    {
        SumBin += GSGlobalHistogram[i];
    }
    
    float MiddleGreyLuminance = g_MiddleGrey * ComputeLuminanceFromLogLuminance(g_ExposureCompensationEV);
    
    if (SumBin < 1.f)
    {
        ExposureScaleBuffer[0] = float4(1.f, 1.f, 0.f, MiddleGreyLuminance);
        return;
    }
    
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
        TotalEv += ComputeLogLuminanceFromHistogramPosition(HistogramPos, g_HistogramScale, g_HistogramBias) * (float)CurrBin;
        TotalCount += (float)CurrBin;
    }
    if (TotalCount < 1.f)
    {
        ExposureScaleBuffer[0] = float4(1.f, 1.f, 0.f, MiddleGreyLuminance);
        return;
    }
    
    float AverEV = TotalEv * rcp(max(TotalCount, 1e-5));
    float AverLuminance = ComputeLuminanceFromLogLuminance(AverEV);
    float ClampAverLuminance = clamp(AverLuminance, g_MinAverageLuminance, g_MaxAverageLuminance);
    
    float ExposureScale = MiddleGreyLuminance / max(ClampAverLuminance, 1e-5);
    float ExposureScaleWithoutEV = g_MiddleGrey * rcp(max(ClampAverLuminance, 1e-5));
    
    // r = scale with EV, g = scale without EV, b = avg lum, a = middleGrey * exp2(EV)
    ExposureScaleBuffer[0] = float4(ExposureScale, ExposureScaleWithoutEV, AverLuminance, MiddleGreyLuminance);
}

// CLEAR_HISTOGRAM_THREAD_GROUP
// void ComputeExposure(uint groupIndex : SV_GroupIndex)
// {
//     RWStructuredBuffer<uint> HistogramBuffer = ResourceDescriptorHeap[g_HistogramBufferIndex];
//     RWStructuredBuffer<float4> ExposureScaleBuffer = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
//     RWTexture2D<float> histogramTex = ResourceDescriptorHeap[g_HistogramTexIndex];
//
//     GSGlobalHistogram[groupIndex] = HistogramBuffer[groupIndex];
//     GroupMemoryBarrierWithGroupSync();
//
//     if (groupIndex < HISTOGRAM_SIZE)
//     {
//         histogramTex[uint2(groupIndex, 0)] = (float)GSGlobalHistogram[groupIndex];
//     }
//
//     if (groupIndex != 0)
//     {
//         return;
//     }
//
//     float middleGrey = max(g_MiddleGrey, 1e-4f);
//     float expComp = exp2(g_ExposureCompensationEV);
//     float biasOnly = middleGrey * expComp;
//
//     float histSum = 0.0f;
//     [unroll]
//     for (uint i = 0; i < HISTOGRAM_SIZE; ++i)
//     {
//         histSum += (float)GSGlobalHistogram[i];
//     }
//     if (histSum < 1.0f)
//     {
//         // r = scale with EV, g = scale without EV, b = avg lum, a = middleGrey * exp2(EV)
//         ExposureScaleBuffer[0] = float4(1.0f, 1.0f, 0.0f, biasOnly);
//         return;
//     }
//
//     float minFractionSum = histSum * saturate(g_LowPercent);
//     float maxFractionSum = histSum * saturate(max(g_HighPercent, g_LowPercent));
//
//     float2 sumWithoutOutliers = 0.0f;
//
//     [unroll]
//     for (uint b = 0; b < HISTOGRAM_SIZE; ++b)
//     {
//         float localValue = (float)GSGlobalHistogram[b];
//
//         float sub = min(localValue, minFractionSum);
//         localValue -= sub;
//         minFractionSum -= sub;
//         maxFractionSum -= sub;
//
//         localValue = min(localValue, maxFractionSum);
//         maxFractionSum -= localValue;
//
//         float histogramPos = (float)b / (float)(HISTOGRAM_SIZE - 1);
//         float logL = ComputeLogLuminanceFromHistogramPosition(histogramPos, g_HistogramScale, g_HistogramBias);
//         sumWithoutOutliers += float2(logL, 1.0f) * localValue;
//     }
//
//     float avgLogL = sumWithoutOutliers.x / max(sumWithoutOutliers.y, 1e-4f);
//     float avgLum = exp2(avgLogL);
//     float clampedLum = clamp(avgLum, g_MinAverageLuminance, g_MaxAverageLuminance);
//
//     float scaleWithoutBias = middleGrey / max(clampedLum, 1e-4f);
//     float exposureScale = expComp * scaleWithoutBias;
//
//     ExposureScaleBuffer[0] = float4(exposureScale, scaleWithoutBias, avgLum, biasOnly);
// }
