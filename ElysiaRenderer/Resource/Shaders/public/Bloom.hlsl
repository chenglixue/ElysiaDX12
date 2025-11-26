#if EDITOR
#include <private\Common.hlsl>
#else
#include "../private\Common.hlsl"
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_DestTextureIndex;
    UINT g_SourceTextureIndex;
    Vector4 g_DestSize;
    Vector4 g_SourceSize;
    
    float g_LuminanceThreshold;
    float g_I_DownSampleCounts;
    float g_BloomIntensity;
}

float3 ApplyThreshold(float3 color, out float luminance);
float GetLuminanceWeight(float luminance);
float GetLuminanceWeight(float3 color);

[numthreads(8, 8, 1)]
void BloomWeightedDownSample(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 screenUV = ((float2) dispatchThreadID.xy + 0.5f) * g_DestSize.zw;
    float2 halfPixel = g_DestSize.zw * 0.5f;
    
    float centerLuminance, topLeftLuminance, topRightLuminance, bottomRightLuminance, bottomLeftLuminance = 0.f;
    float3 center = ApplyThreshold(SampleTexture2D(g_SourceTextureIndex, screenUV, WarpLinearSampler),
        centerLuminance);
    float3 topLeft = ApplyThreshold(SampleTexture2D(g_SourceTextureIndex, screenUV + halfPixel * float2(-1.f, 1.f), WarpLinearSampler),
        topLeftLuminance);
    float3 topRight = ApplyThreshold(SampleTexture2D(g_SourceTextureIndex, screenUV + halfPixel * float2(1.f, 1.f), WarpLinearSampler),
        topRightLuminance);
    float3 bottomRight = ApplyThreshold(SampleTexture2D(g_SourceTextureIndex, screenUV + halfPixel * float2(1.f, -1.f), WarpLinearSampler),
        bottomRightLuminance);
    float3 bottomLeft = ApplyThreshold(SampleTexture2D(g_SourceTextureIndex, screenUV + halfPixel * float2(-1.f, -1.f), WarpLinearSampler),
        bottomLeftLuminance);
    
    float centerWeight = GetLuminanceWeight(centerLuminance);
    float topLeftWeight = GetLuminanceWeight(topLeftLuminance);
    float topRightWeight = GetLuminanceWeight(topRightLuminance);
    float bottomRightWeight = GetLuminanceWeight(bottomRightLuminance);
    float bottomLeftWeight = GetLuminanceWeight(bottomLeftLuminance);
    
    float3 colorSum = center * centerWeight * 4.f + topLeft * topLeftWeight + topRight * topRightWeight + bottomRight * bottomRightWeight + bottomLeft * bottomLeftWeight;
    float3 weightSum = centerWeight * 4.f + topLeftWeight + topRightWeight + bottomRightWeight + bottomLeftWeight;
    float3 blurColor = colorSum / weightSum;
    
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_DestTextureIndex];
    
    o[dispatchThreadID.xy] = float4(blurColor, 1.f);
}

[numthreads(8, 8, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 screenUV = ((float2) dispatchThreadID.xy + 0.5f) * g_DestSize.zw;
    
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_DestTextureIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    //o[dispatchThreadID.xy] = SampleTexture2D(g_DestTextureIndex, screenUV, WarpLinearSampler);
    o[dispatchThreadID.xy] = LoadTexture2D(g_DestTextureIndex, dispatchThreadID.xy);
}

float3 ApplyThreshold(float3 color, out float luminance)
{
    luminance = Luminance(color);

    return color * max(0.f, luminance - g_LuminanceThreshold);
}

float GetLuminanceWeight(float luminance)
{
    return rcp(1.f + luminance);
}

float GetLuminanceWeight(float3 color)
{
    float luminance = Luminance(color);

    return rcp(1.f + luminance);
}