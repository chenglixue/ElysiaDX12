#include "private\ShadingCommon.hlsl"

#define MAX_BLUR_RADIUS 10

#define BLUR_GROUP_SIZE 64

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    UINT g_HIZTextureIndex;
    float4 g_TargetSize;
    float g_Sharpness;
    UINT g_BlurRadius;
    float g_Weights[MAX_BLUR_RADIUS + 1];
}

const static float Constant_Float16F_Scale = 4096.0f * 32.0f;

float ComputeWeight(float sampleEyeDepth,
                    float3 sampleNormal,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float blurRadius);
void ComputeBlur(float2 centerUV,
                 float centerEyeDepth,
                 float3 centerNormal,
                 float2 direction,
                 float2 texelSize,
                 float distance,
                 bool useBilinear,
                 inout float totalAO,
                 inout float totalWeight);
void ComputeDirBlur(float2 centerUV,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float2 dir,
                    float2 texelSize,
                    float blurRadius,
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

    float4 HIZNormal = SampleTexture2D_LOD(g_HIZTextureIndex,
                                           screenUV,
                                           ClampPointSampler,
                                           0);

    FInputParams inputParam = (FInputParams)0;
    inputParam.ScreenUV = screenUV;
    inputParam.NormalWS = HIZNormal.rgb;
    inputParam.LinearEyeDepth = HIZNormal.a * Constant_Float16F_Scale;

    float centerAO = SampleTexture2D(g_SourceTexIndex,
                                     screenUV,
                                     ClampPointSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float3 centerNormal = inputParam.NormalWS;

    float totalAO = centerAO;
    float totalWeight = 1;

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(1, 0),
                   g_TargetSize.zw,
                   g_BlurRadius,
                   totalAO,
                   totalWeight);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(-1, 0),
                   g_TargetSize.zw,
                   g_BlurRadius,
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
    float4 HIZNormal = SampleTexture2D_LOD(g_HIZTextureIndex,
                                           screenUV,
                                           ClampPointSampler,
                                           0);

    FInputParams inputParam = (FInputParams)0;
    inputParam.NormalWS = HIZNormal.rgb;
    inputParam.LinearEyeDepth = HIZNormal.a * Constant_Float16F_Scale;

    float centerAO = SampleTexture2D(g_SourceTexIndex,
                                     screenUV,
                                     ClampPointSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float3 centerNormal = inputParam.NormalWS;

    float totalAO = centerAO;
    float totalWeight = 1;

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(0.f, 1.f),
                   g_TargetSize.zw,
                   g_BlurRadius,
                   totalAO,
                   totalWeight);

    ComputeDirBlur(inputParam.ScreenUV,
                   centerEyeDepth,
                   centerNormal,
                   float2(0.f, -1.f),
                   g_TargetSize.zw,
                   g_BlurRadius,
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
                    inout float totalAO,
                    inout float totalWeight)
{
    [branch]
    if (blurRadius > 4)
    {
        for (float r = 1.f; r <= blurRadius * 0.5f; r += 1.f)
        {
            ComputeBlur(centerUV,
                        centerEyeDepth,
                        centerNormal,
                        dir,
                        texelSize,
                        r,
                        false,
                        totalAO,
                        totalWeight);
        }

        for (float r = blurRadius * 0.5f + 1.f; r <= blurRadius; r += 2.f)
        {
            ComputeBlur(centerUV,
                        centerEyeDepth,
                        centerNormal,
                        dir,
                        texelSize,
                        r,
                        true,
                        totalAO,
                        totalWeight);
        }
    }
    else
    {
        for (float r = 1.f; r <= blurRadius; r += 1.f)
        {
            ComputeBlur(centerUV,
                        centerEyeDepth,
                        centerNormal,
                        dir,
                        texelSize,
                        r,
                        false,
                        totalAO,
                        totalWeight);
        }
    }
}

void ComputeBlur(float2 centerUV,
                 float centerEyeDepth,
                 float3 centerNormal,
                 float2 direction,
                 float2 texelSize,
                 float distance,
                 bool useBilinear,
                 inout float totalAO,
                 inout float totalWeight)
{
    float2 sampleUV;
    float sampleAO;
    float sampleEyeDepth;
    float3 sampleNormal;
    float LOD = 0;

    [branch]
    if (useBilinear)
    {
        // sample [r, r+1]
        float2 uv0 = centerUV + (distance + 0.5f) * texelSize * direction;
        sampleAO = SampleTexture2D(g_SourceTexIndex, uv0, ClampLinearSampler).r;

        float2 uv1 = centerUV + distance * direction * texelSize;
        float2 uv2 = centerUV + (distance + 1.0) * direction * texelSize;
        float4 HIZNormal1 = SampleTexture2D_LOD(g_HIZTextureIndex, uv1, ClampPointSampler, LOD);
        float4 HIZNormal2 = SampleTexture2D_LOD(g_HIZTextureIndex, uv2, ClampPointSampler, LOD);
        float eyeDepth1 = HIZNormal1.a * Constant_Float16F_Scale;
        float eyeDepth2 = HIZNormal2.a * Constant_Float16F_Scale;
        sampleEyeDepth = (eyeDepth1 + eyeDepth2) * 0.5;

        float3 normal1 = HIZNormal1.xyz;
        float3 normal2 = HIZNormal2.xyz;
        sampleNormal = normalize(normal1 + normal2);
    }
    else
    {
        sampleUV = centerUV + direction * distance * texelSize;
        sampleAO = SampleTexture2D(g_SourceTexIndex,
                                   sampleUV,
                                   ClampPointSampler).r;
        float4 HIZNormal = SampleTexture2D_LOD(g_HIZTextureIndex, sampleUV, ClampPointSampler, LOD);
        sampleEyeDepth = HIZNormal.a * Constant_Float16F_Scale;
        sampleNormal = HIZNormal.rgb;
    }

    float weight = ComputeWeight(sampleEyeDepth, sampleNormal, centerEyeDepth, centerNormal, distance);
    totalAO += sampleAO * weight;
    totalWeight += weight;
}

float ComputeWeight(float sampleEyeDepth,
                    float3 sampleNormal,
                    float centerEyeDepth,
                    float3 centerNormal,
                    float blurRadius)
{
    float sigma = g_BlurRadius * 0.5;
    float spatialWeight;
    spatialWeight = exp(-blurRadius * blurRadius / (2.0 * sigma * sigma));

    float depthDiff = abs(centerEyeDepth - sampleEyeDepth);
    float depthScale = 10.0f / max(1.0f, centerEyeDepth);
    float depthWeight = exp(-depthDiff * depthScale);

    float normalDiff = saturate(dot(centerNormal, sampleNormal));
    float normalWeight = pow(max(normalDiff, 0.1f), g_Sharpness);

    float finalWeight = spatialWeight * depthWeight * normalWeight;

    return finalWeight;
}