#include "private\ShadingCommon.hlsl"

#define MAX_BLUR_RADIUS 4

#define BLUR_GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float2 g_BlurDir;
    float4 g_TargetSize;
    float g_Sharpness;
    UINT g_BlurRadius;
    float g_BlurIntensity;
    float g_Weights[MAX_BLUR_RADIUS + 1];
}

[numthreads(BLUR_GROUP_SIZE, 1, 1)]
void HorizionBilateralBlur(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= UINT(g_TargetSize.x) || dispatchThreadID.y >= UINT(g_TargetSize.y))
    {
        return;
    }
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);
    FInputParams inputParam = (FInputParams)0;
    inputParam.ScreenUV = screenUV;
    inputParam.PixelPos = screenUV * g_TargetSize.xy;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.Linear01Depth = Linear01Depth(GBufferData.Depth, g_ZBufferParams);
    inputParam.LinearEyeDepth = LinearEyeDepth(GBufferData.Depth, g_ZBufferParams);

    float centerAO = SampleTexture2D(g_SourceTexIndex, screenUV, ClampLinearSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float centerNormal = inputParam.NormalWS;

    float totalAO = centerAO * g_Weights[0];
    float totalWeight = g_Weights[0];

    [unroll(4)]
    for (UINT i = 1; i <= g_BlurRadius; i ++)
    {
        float2 offset = g_BlurDir * i * g_TargetSize.zw * g_BlurIntensity;
        float2 uv[2] = {screenUV - offset, screenUV + offset};

        for (int j = 0; j < 2; j ++)
        {
            float sampleAO = SampleTexture2D(g_SourceTexIndex, uv[j], ClampLinearSampler).r;
            float sampleNormal = DecodeNormal(SampleTexture2D(GBuffer3Index, uv[j], ClampLinearSampler).rgb);
            sampleNormal = normalize(sampleNormal);
            float sampleDepth = SampleTexture2D(OpaqueDepthIndex, uv[j], ClampPointSampler).r;
            float sampleEyeDepth = LinearEyeDepth(sampleDepth, g_ZBufferParams);

            float depthDiff = abs(centerEyeDepth - sampleEyeDepth);
            // 深度差越大，weight接近0，保留更多边缘细节
            float depthWeight = exp(-depthDiff * g_Sharpness);

            float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), 16.f);

            float finalWeight = g_Weights[i] * depthWeight * normalWeight;

            totalAO += sampleAO * finalWeight;
            totalWeight += finalWeight;
        }
    }

    float result = totalAO / (totalWeight + 0.0001f);
    o[dispatchThreadID.xy] = float4(result, 0, 0, 1);
}

[numthreads(1, BLUR_GROUP_SIZE, 1)]
void VerticalBilateralBlur(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= UINT(g_TargetSize.x) || dispatchThreadID.y >= UINT(g_TargetSize.y))
    {
        return;
    }
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);
    FInputParams inputParam = (FInputParams)0;
    inputParam.ScreenUV = screenUV;
    inputParam.PixelPos = screenUV * g_TargetSize.xy;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.Linear01Depth = Linear01Depth(GBufferData.Depth, g_ZBufferParams);
    inputParam.LinearEyeDepth = LinearEyeDepth(GBufferData.Depth, g_ZBufferParams);

    float centerAO = SampleTexture2D(g_SourceTexIndex, screenUV, ClampLinearSampler).r;
    float centerEyeDepth = inputParam.LinearEyeDepth;
    float centerNormal = inputParam.NormalWS;

    float totalAO = centerAO * g_Weights[0];
    float totalWeight = g_Weights[0];

    [unroll(4)]
    for (UINT i = 1; i <= g_BlurRadius; i ++)
    {
        float2 offset = g_BlurDir * i * g_TargetSize.zw * g_BlurIntensity;
        float2 uv[2] = {screenUV - offset, screenUV + offset};

        for (int j = 0; j < 2; j ++)
        {
            float sampleAO = SampleTexture2D(g_SourceTexIndex, uv[j], ClampLinearSampler).r;
            float sampleNormal = DecodeNormal(SampleTexture2D(GBuffer3Index, uv[j], ClampLinearSampler).rgb);
            sampleNormal = normalize(sampleNormal);
            float sampleDepth = SampleTexture2D(OpaqueDepthIndex, uv[j], ClampPointSampler).r;
            float sampleEyeDepth = LinearEyeDepth(sampleDepth, g_ZBufferParams);

            float depthDiff = abs(centerEyeDepth - sampleEyeDepth);
            // 深度差越大，weight接近0，保留更多边缘细节
            float depthWeight = exp(-depthDiff * g_Sharpness);

            float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), 16.f);

            float finalWeight = g_Weights[i] * depthWeight * normalWeight;

            totalAO += sampleAO * finalWeight;
            totalWeight += finalWeight;
        }
    }

    float result = totalAO / (totalWeight + 0.0001f);
    o[dispatchThreadID.xy] = float4(result, 0, 0, 1);
}