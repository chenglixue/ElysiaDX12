#include "private\ShadingCommon.hlsl"

#define MAX_BLUR_RADIUS 10

#define BLUR_GROUP_SIZE 64

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float4 g_TargetSize;
    float g_Sharpness;
    UINT g_BlurRadius;

    UINT g_AOImportanceTexIndex;
}
static const float GAUSSIAN_WEIGHTS[MAX_BLUR_RADIUS + 1] = {
    1.000000,
    0.939413,
    0.778801,
    0.569783,
    0.367879,
    0.209611,
    0.105399,
    0.046771,
    0.018316,
    0.006383,
    0.001930
};


const static float Constant_Float16F_Scale = 4096.0f * 32.0f;

float ComputeWeight(float sampleEyeDepth,
                    float3 sampleNormal,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float blurRadius,
                    float blurFalloff);
void ComputeBlur(float2 centerUV,
                 float centerEyeDepth,
                 float3 centerNormal,
                 float2 direction,
                 float2 texelSize,
                 float distance,
                 float blurFalloff,
                 float stride,
                 bool useBilinear,
                 inout float totalAO,
                 inout float totalWeight);
void ComputeDirBlur(float2 centerUV,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float2 dir,
                    float2 texelSize,
                    float blurRadius,
                    float stride,
                    float blurFalloff,
                    inout float totalAO,
                    inout float totalWeight);

[numthreads(BLUR_GROUP_SIZE, 1, 1)]
void HorizionBilateralBlur(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= UINT(g_TargetSize.x) || dispatchThreadID.y >=
        UINT(g_TargetSize.y))
    {
        return;
    }
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler).r;

    FInputParams inputParam = (FInputParams)0;
    inputParam.ScreenUV = screenUV;
    inputParam.NormalWS = SampleNormalWS(screenUV, ClampPointSampler);
    inputParam.LinearEyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);

    float centerAO = SampleTexture2D(g_SourceTexIndex,
                                     screenUV,
                                     ClampPointSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float3 centerNormal = inputParam.NormalWS;

    float totalAO = centerAO;
    float totalWeight = 1;

    float importance = SampleTexture2D(g_AOImportanceTexIndex, screenUV, ClampPointSampler).r;
    float dynamicRadius = lerp((float)g_BlurRadius, 1.0f, importance);
    float dynamicStride = lerp(3.0f, 1.0f, importance);
    float sigma = (dynamicRadius + 1.0f) * 0.5f;
    float blurFalloff = 1.0f / (2.0f * sigma * sigma);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(1, 0),
                   g_TargetSize.zw,
                   dynamicRadius,
                   dynamicStride,
                   blurFalloff,
                   totalAO,
                   totalWeight);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(-1, 0),
                   g_TargetSize.zw,
                   dynamicRadius,
                   dynamicStride,
                   blurFalloff,
                   totalAO,
                   totalWeight);

    float result = totalAO / (totalWeight + 1e-5);
    o[dispatchThreadID.xy] = result;
}

[numthreads(1, BLUR_GROUP_SIZE, 1)]
void VerticalBilateralBlur(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= UINT(g_TargetSize.x) || dispatchThreadID.y >=
        UINT(g_TargetSize.y))
    {
        return;
    }
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;
    float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler).r;
    float importance = SampleTexture2D(g_AOImportanceTexIndex, screenUV, ClampPointSampler).r;

    FInputParams inputParam = (FInputParams)0;
    inputParam.ScreenUV = screenUV;
    inputParam.NormalWS = SampleNormalWS(screenUV, ClampPointSampler);
    inputParam.LinearEyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);

    float centerAO = SampleTexture2D(g_SourceTexIndex,
                                     screenUV,
                                     ClampPointSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float3 centerNormal = inputParam.NormalWS;

    float totalAO = centerAO;
    float totalWeight = 1;

    float dynamicRadius = lerp((float)g_BlurRadius, 1.0f, importance);
    float dynamicStride = lerp(3.f, 2.0f, importance);
    float sigma = (dynamicRadius + 1.0f) * 0.5f;
    float blurFalloff = 1.0f / (2.0f * sigma * sigma);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(0.f, 1.f),
                   g_TargetSize.zw,
                   dynamicRadius,
                   dynamicStride,
                   blurFalloff,
                   totalAO,
                   totalWeight);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(0.f, -1.f),
                   g_TargetSize.zw,
                   dynamicRadius,
                   dynamicStride,
                   blurFalloff,
                   totalAO,
                   totalWeight);

    float result = totalAO / (totalWeight + 1e-5);
    o[dispatchThreadID.xy] = result;
}

void ComputeDirBlur(float2 centerUV,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float2 dir,
                    float2 texelSize,
                    float blurRadius,
                    float stride,
                    float blurFalloff,
                    inout float totalAO,
                    inout float totalWeight)
{
    [loop]
    for (float r = 1.f; r <= blurRadius; r += 1.f)
    {
        ComputeBlur(centerUV,
                    centerEyeDepth,
                    centerNormal,
                    dir,
                    texelSize,
                    r,
                    stride,
                    blurFalloff,
                    false,
                    totalAO,
                    totalWeight);
    }

    // [branch]
    // if (blurRadius > 4)
    // {
    //     for (float r = 1.f; r <= blurRadius * 0.5f; r += 1.f)
    //     {
    //         ComputeBlur(centerUV,
    //                     centerEyeDepth,
    //                     centerNormal,
    //                     dir,
    //                     texelSize,
    //                     r,
    //                     stride,
    //                     false,
    //                     totalAO,
    //                     totalWeight);
    //     }
    //
    //     for (float r = blurRadius * 0.5f + 1.f; r <= blurRadius; r += 2.f)
    //     {
    //         ComputeBlur(centerUV,
    //                     centerEyeDepth,
    //                     centerNormal,
    //                     dir,
    //                     texelSize,
    //                     r,
    //                     stride,
    //                     true,
    //                     totalAO,
    //                     totalWeight);
    //     }
    // }
    // else
    // {
    //     for (float r = 1.f; r <= blurRadius; r += 1.f)
    //     {
    //         ComputeBlur(centerUV,
    //                     centerEyeDepth,
    //                     centerNormal,
    //                     dir,
    //                     texelSize,
    //                     r,
    //                     stride,
    //                     false,
    //                     totalAO,
    //                     totalWeight);
    //     }
    // }
}

void ComputeBlur(float2 centerUV,
                 float centerEyeDepth,
                 float3 centerNormal,
                 float2 direction,
                 float2 texelSize,
                 float distance,
                 float stride,
                 float blurFalloff,
                 bool useBilinear,
                 inout float totalAO,
                 inout float totalWeight)
{
    float2 sampleUV;
    float sampleAO;
    float sampleEyeDepth;
    float3 sampleNormal;
    float LOD = 0;
    float sampleRadius = 1.f;

    sampleUV = centerUV + direction * stride * distance * texelSize * sampleRadius;
    sampleAO = SampleTexture2D(g_SourceTexIndex,
                               sampleUV,
                               ClampPointSampler).r;
    float rawDepth = SampleTexture2D(OpaqueDepthIndex, sampleUV, ClampPointSampler).r;
    sampleEyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);
    sampleNormal = SampleNormalWS(sampleUV, ClampPointSampler);

    // [branch]
    // if (useBilinear)
    // {
    //     float2 uv0 = centerUV + (distance * stride + 0.5f) * texelSize * direction * sampleRadius;
    //     sampleAO = SampleTexture2D(g_SourceTexIndex, uv0, ClampLinearSampler).r;
    //
    //     float2 uv1 = centerUV + distance * stride * direction * texelSize * sampleRadius;
    //     float2 uv2 = centerUV + (distance * stride + 1.0) * direction * texelSize * sampleRadius;
    //     float rawDepth1 = SampleTexture2D(OpaqueDepthIndex, uv1, ClampPointSampler).r;
    //     float rawDepth2 = SampleTexture2D(OpaqueDepthIndex, uv2, ClampPointSampler).r;
    //     float eyeDepth1 = LinearEyeDepth(rawDepth1, g_ZBufferParams);
    //     float eyeDepth2 = LinearEyeDepth(rawDepth2, g_ZBufferParams);
    //     sampleEyeDepth = (eyeDepth1 + eyeDepth2) * 0.5;
    //
    //     float3 normal1 = SampleNormalWS(uv1, ClampPointSampler);
    //     float3 normal2 = SampleNormalWS(uv1, ClampPointSampler);
    //     sampleNormal = normalize(normal1 + normal2);
    // }
    // else
    // {
    //     sampleUV = centerUV + direction * stride * distance * texelSize * sampleRadius;
    //     sampleAO = SampleTexture2D(g_SourceTexIndex,
    //                                sampleUV,
    //                                ClampPointSampler).r;
    //     float rawDepth = SampleTexture2D(OpaqueDepthIndex, sampleUV, ClampPointSampler).r;
    //     sampleEyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);
    //     sampleNormal = SampleNormalWS(sampleUV, ClampPointSampler);
    // }

    float weight = ComputeWeight(sampleEyeDepth,
                                 sampleNormal,
                                 centerEyeDepth,
                                 centerNormal,
                                 distance,
                                 blurFalloff);
    totalAO += sampleAO * weight;
    totalWeight += weight;
}

float ComputeWeight(float sampleEyeDepth,
                    float3 sampleNormal,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float blurRadius,
                    float blurOff)
{
    float spatialWeight = exp2(-Pow2(blurRadius) * blurOff);

    float depthDiff = abs(centerEyeDepth - sampleEyeDepth);
    float depthScale = 10.0f / max(1.0f, centerEyeDepth);
    float depthWeight = exp(-depthDiff * depthScale);

    float normalDiff = saturate(dot(centerNormal, sampleNormal));
    float normalWeight = pow(max(normalDiff, 0.1f), g_Sharpness);

    float finalWeight = spatialWeight * depthWeight * normalWeight;

    return finalWeight;
}