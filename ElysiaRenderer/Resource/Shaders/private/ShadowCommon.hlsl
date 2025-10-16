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

int4 SampleShadowPCF(in float2 baseUV, in float depth,
                   in Texture2D shadowMap, in SamplerState pcfSampler,
                    in uint arrayIdx = 0, in int2 offsetUV = int2(0, 0))
{
    float2 uv = baseUV;
    
    float4 samples = shadowMap.GatherRed(pcfSampler, baseUV, offsetUV);
    
    int4 isShadow = int4(step(depth, samples.x),
        step(depth, samples.y),
        step(depth, samples.z),
        step(depth, samples.w));
    
    return isShadow;
}

inline float PCF1x1(float2 fraction, float4 isShadow)
{
    float2 horizontalLerp = lerp(isShadow.wx, isShadow.zy, fraction.xx);

    return lerp(horizontalLerp.x, horizontalLerp.y, fraction.y);
}

inline float PCF3X3(float2 fraction, float4 isShadow0, float4 isShadow1, float4 isShadow2, float4 isShadow3)
{
    float4 Results;

    Results.x = isShadow0.w * (1.0 - fraction.x);
    Results.y = isShadow0.x * (1.0 - fraction.x);
    Results.z = isShadow2.w * (1.0 - fraction.x);
    Results.w = isShadow2.x * (1.0 - fraction.x);
    Results.x += isShadow0.z;
    Results.y += isShadow0.y;
    Results.z += isShadow2.z;
    Results.w += isShadow2.y;
    Results.x += isShadow1.w;
    Results.y += isShadow1.x;
    Results.z += isShadow3.w;
    Results.w += isShadow3.x;
    Results.x += isShadow1.z * fraction.x;
    Results.y += isShadow1.y * fraction.x;
    Results.z += isShadow3.z * fraction.x;
    Results.w += isShadow3.y * fraction.x;

    return dot(Results, float4(1.0 - fraction.y, 1.0, 1.0, fraction.y) * (1.0 / 9.0));
}

float2 HorizontalPCF5x2(float2 Fraction, float4 Values00, float4 Values20, float4 Values40)
{
    float Results0;
    float Results1;

    Results0 = Values00.w * (1.0 - Fraction.x);
    Results1 = Values00.x * (1.0 - Fraction.x);
    Results0 += Values00.z;
    Results1 += Values00.y;
    Results0 += Values20.w;
    Results1 += Values20.x;
    Results0 += Values20.z;
    Results1 += Values20.y;
    Results0 += Values40.w;
    Results1 += Values40.x;
    Results0 += Values40.z * Fraction.x;
    Results1 += Values40.y * Fraction.x;

    return float2(Results0, Results1);
}

inline float Manual1x1PCF(float2 shadowPos, float lightDepth, Texture2D shadowTex, in SamplerState pcfSampler, uint CSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowSize.xy;
    texelPos -= 0.5f;
    
    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowSize.zw;
    
    int4 isShadow = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler);
    
    return PCF1x1(fraction, isShadow);
}

inline float Manual3x3PCF(float2 shadowPos, float lightDepth, Texture2D shadowTex, in SamplerState pcfSampler, uint CSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowSize.xy;
    texelPos -= 0.5f;
    
    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowSize.zw;
    
    int4 isShadow0 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(-1, -1));
    int4 isShadow1 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(1, -1));
    int4 isShadow2 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(-1, 1));
    int4 isShadow3 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(1, 1));
    
    return PCF3X3(fraction, isShadow0, isShadow1, isShadow2, isShadow3);

}

inline float Manual5x5PCF(float2 shadowPos, float lightDepth, Texture2D shadowTex, in SamplerState pcfSampler, uint CSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowSize.xy;
    texelPos -= 0.5f;
    
    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowSize.zw;
    int step = 2;
    
    int4 isShadow00 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(-step, -step));
    int4 isShadow20 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(0, -step));
    int4 isShadow40 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(step, -step));
    
    float2 Row0 = HorizontalPCF5x2(fraction, isShadow00, isShadow20, isShadow40);
    float Results = Row0.x * (1.0f - fraction.y) + Row0.y;
    
    int4 isShadow02 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(-step, 0));
    int4 isShadow22 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(0, 0));
    int4 isShadow42 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(step, 0));
    
    float2 Row1 = HorizontalPCF5x2(fraction, isShadow02, isShadow22, isShadow42);
    Results += Row1.x + Row1.y;
    
    float4 Values04 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(-step, step));
    float4 Values24 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(0, step));
    float4 Values44 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, CSMIndex, int2(step, step));

    float2 Row2 = HorizontalPCF5x2(fraction, Values04, Values24, Values44);
    Results += Row2.x + Row2.y * fraction.y;
    
    return 0.04 * Results;
}

float SampleShadowPCF(in Texture2D shadowMap,
                      in float4 shadowPos, in UINT arrayIndex)
{
    float o = 0.f;
    
    SamplerComparisonState compShadowSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    SamplerState pointShadowSampler = SamplerDescriptorHeap[ClampPointSampler];
    
#if defined(HARD_SHADOW)
    o = shadowMap.SampleCmpLevelZero(compShadowSampler, shadowPos.xy, shadowPos.z);
#else
    #if defined (SHADOW_QUALITY_LOW)
    o = Manual1x1PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler);
    #elif defined (SHADOW_QUALITY_MIDDLE)
    o = Manual3x3PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler);
    #elif defined (SHADOW_QUALITY_HIGH)
    o = Manual5x5PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler);
    #elif defined (SHADOW_QUALITY_VERYHIGH)
    #endif

#endif
    
    return o;
}

float SunShadowVisibility(in float3 positionWS, in float2 uvOffset)
{
    Texture2D shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerState shadowClampSampler = SamplerDescriptorHeap[ClampPointSampler];
    
    float4 shadowPos = mul(shadowMatrix, float4(positionWS, 1.f));
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    return SampleShadowPCF(shadowTex, shadowPos, 0);
}

#endif