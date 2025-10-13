#ifndef SHADOW_COMMON_H
#define SHADOW_COMMON_H

#include "../private\SharedCommon.hlsli"

//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
float GetShadowDepthOffset(in float NoL, inout float4 positionCS, in float shadowMapSize)
{
    const float slope = clamp(abs(NoL) > 0 ? sqrt(saturate(1.f - Pow2(NoL))) / NoL : shadowMaxSlopeDepthBias, 0, shadowMaxSlopeDepthBias);
    
    const float slopeBias = shadowSlopeDepthBias * slope;
    const float constantDepthBias = shadowDepthBias;
    
    const float depthBias = slopeBias + constantDepthBias;
    
    positionCS.z = positionCS.z + depthBias;
    
    return positionCS.z;
}

float SampleShadow(in float2 baseUV, in float u, in float v, in float2 shadowMapSizeInv,
                   in uint arrayIdx, in float depth, 
                   in Texture2D shadowMap,
                   in SamplerComparisonState pcfSampler)
{
    float2 uv = baseUV + float2(u, v) * shadowMapSizeInv;
    
    float shadow = shadowMap.SampleCmpLevelZero(pcfSampler, baseUV, depth);
    
    return shadow;
}

float SampleShadowPCF(in Texture2D shadowMap, in SamplerComparisonState pcfSampler,
                      in float4 shadowPos, in UINT arrayIndex)
{
    float o = 0.f;
    
    Texture2D<float> shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerComparisonState shadowClampSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    
    float2 uv = shadowPos.xy * shadowSize.xy;
    float2 shadowMapSizeInv = shadowSize.zw;
    
    float2 baseUV = floor(uv + 0.5f);

    float s = (uv.x + 0.5f - baseUV.x);
    float t = (uv.y + 0.5f - baseUV.y);

    baseUV -= float2(0.5f, 0.5f);
    baseUV *= shadowMapSizeInv;
    
    #if SHADOW_QUALITY_LOW 
    o = SampleShadow();
    #endif
    
    
    
    return o;
}

float SunShadowVisibility(in float3 positionWS, in float2 uvOffset)
{
    Texture2D<float> shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerComparisonState shadowClampSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    
    float4 shadowPos = mul(shadowMatrix, float4(positionWS, 1.f));
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    return SampleShadowPCF(shadowTex, shadowClampSampler, shadowPos, 0);

}

#endif