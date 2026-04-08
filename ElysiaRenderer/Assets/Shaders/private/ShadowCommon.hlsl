#ifndef SHADOW_COMMON_H
#define SHADOW_COMMON_H

#include "SharedCommon.hlsli"
#include "Random.hlsl"
#include "ShadowConst.hlsli"

//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
float GetShadowDepthOffset(in float NoL,
                           inout float4 positionCS,
                           in float4 shadowMapSize,
                           in float shadowDepthBias,
                           in float shadowSlopeDepthBias,
                           in float shadowMaxSlopeDepthBias)
{
    const float slope = clamp(abs(NoL) > 0 ? sqrt(saturate(1.f - Pow2(NoL))) / NoL : shadowMaxSlopeDepthBias,
                              0,
                              shadowMaxSlopeDepthBias);

    const float slopeBias = shadowSlopeDepthBias * slope;
    const float constantDepthBias = shadowDepthBias;

    const float depthBias = slopeBias + constantDepthBias;

    positionCS.z = positionCS.z + depthBias;

    return positionCS.z;
}

int4 SampleShadowPCF(in float2 baseUV,
                     in float depth,
                     in Texture2D shadowMap,
                     in SamplerState pcfSampler,
                     in uint arrayIdx = 0,
                     in int2 offsetUV = int2(0, 0))
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

inline float Manual1x1PCF(float2 shadowPos,
                          float lightDepth,
                          Texture2D shadowTex,
                          in SamplerState pcfSampler,
                          in float4 shadowMapSize,
                          uint CSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowMapSize.xy;
    texelPos -= 0.5f;

    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowMapSize.zw;

    int4 isShadow = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler);

    return PCF1x1(fraction, isShadow);
}

inline float Manual3x3PCF(float2 shadowPos,
                          float lightDepth,
                          Texture2D shadowTex,
                          in SamplerState pcfSampler,
                          in float4 shadowMapSize,
                          uint uintCSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowMapSize.xy;
    texelPos -= 0.5f;

    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowMapSize.zw;

    int4 isShadow0 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(-1, -1));
    int4 isShadow1 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(1, -1));
    int4 isShadow2 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(-1, 1));
    int4 isShadow3 = SampleShadowPCF(samplePos, lightDepth, shadowTex, pcfSampler, 0, int2(1, 1));

    return PCF3X3(fraction, isShadow0, isShadow1, isShadow2, isShadow3);

}

inline float Manual5x5PCF(float2 shadowPos,
                          float lightDepth,
                          Texture2D shadowTex,
                          in SamplerState pcfSampler,
                          in float4 shadowMapSize,
                          uint CSMIndex = 0)
{
    float2 texelPos = shadowPos.xy * shadowMapSize.xy;
    texelPos -= 0.5f;

    float2 fraction = frac(texelPos);
    float2 quadCenter = floor(texelPos) + 1.f;
    float2 samplePos = quadCenter * shadowMapSize.zw;
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

inline float2 GetSobolSample(float2 baseSobol, uint2 pixelCoord)
{
    float temporalShift = frac(frameIndex * 0.61803398875f);;
    uint2 seed = hash_int(pixelCoord ^ uint2(frameIndex * 0x9E3779B9, frameIndex * 0x45D9F3B));

    float2 shift = float2(seed) * (1.0 / 4294967295.0);
    shift += temporalShift;

    return frac(baseSobol + shift);
}

inline float2 WarpToDisk(float2 samplePoint)
{
    float2 u = 2.0 * samplePoint - 1.0;

    return u * sqrt(1.0f - 0.5f * u.yx * u.yx);
}

inline float2 GetHeitzBlueNoise(uint2 pixelCoord, uint frameIndex)
{
    uint tx = pixelCoord.x % 128;
    uint ty = pixelCoord.y % 128;
    uint tileBaseIndex = (ty * 128 + tx) * 8;

    uint rank0 = g_RankingTile[tileBaseIndex + 0];
    uint rank1 = g_RankingTile[tileBaseIndex + 1];
    uint scram0 = g_ScramblingTile[tileBaseIndex + 0];
    uint scram1 = g_ScramblingTile[tileBaseIndex + 1];

    uint sampleIndex = frameIndex % 256;

    uint index0 = sampleIndex ^ rank0;
    uint index1 = sampleIndex ^ rank1;

    uint sobol0 = g_Sobol_256spp_256d[index0 * 256 + 0];
    uint sobol1 = g_Sobol_256spp_256d[index1 * 256 + 1];
    sobol0 = sobol0 & 0xFF;
    sobol1 = sobol1 & 0xFF;

    uint val0 = sobol0 ^ scram0;
    uint val1 = sobol1 ^ scram1;

    float2 result;
    result.x = (float)val0 * (1.0f / 256.0f);
    result.y = (float)val1 * (1.0f / 256.0f);

    return result;
}

inline float SobolPCF(float2 screenSize,
                      float2 screenUV,
                      float shadowRadius,
                      float2 shadowPos,
                      float lightDepth,
                      Texture2D shadowTex,
                      in SamplerState pcfSampler,
                      in float4 shadowMapSize,
                      float2 sobolSequence[64],
                      uint CSMIndex = 0)
{
    float shadow = 0.0;
    const UINT numSamples = 3;

    uint2 pixelCoord = uint2(screenUV * screenSize);
    float temporalShift = frac(frameIndex * 0.61803398875f);
    uint2 seed = hash_int(pixelCoord ^ uint2(frameIndex * 0x9E3779B9, frameIndex * 0x45D9F3B));
    float2 shift = float2(seed) * (1.0 / 4294967295.0);
    shift += temporalShift;
    SamplerComparisonState compShadowSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];

    [unroll]
    for (int i = 0; i < numSamples; i ++)
    {
        uint sampleIdx = (i + frameIndex) % 256;
        uint sobolX = g_Sobol_256spp_256d[sampleIdx * 256 + 0];
        uint sobolY = g_Sobol_256spp_256d[sampleIdx * 256 + 1];
        float2 baseSobol = float2(sobolX, sobolY) * (1.0f / 256.0f);

        // float2 baseSobol = sobolSequence[(i + frameIndex) % 64];
        float2 samplePoint = frac(baseSobol + shift);

        float2 offset = WarpToDisk(samplePoint);
        float2 sampleUV = shadowPos + offset * shadowMapSize.zw * shadowRadius;
        float sampleShadow = shadowTex.SampleCmpLevelZero(compShadowSampler, sampleUV, lightDepth);

        shadow += sampleShadow;
    }
    return shadow / numSamples;
}

float sobolNoise(float2 uv, int index)
{
    // Simple Sobol sequence generation for demonstration purposes.
    // In practice, you would use a more robust implementation.
    uint i = uint(index);
    uint result = 0u;
    for (uint j = 0u; j < 32u; ++j)
    {
        if ((i & (1u << j)) != 0u)
        {
            result ^= 0x80000000u >> j;
        }
    }
    return float(result) / float(0xFFFFFFFFu);
}

float SampleShadowPCF(in Texture2D shadowMap,
                      in float4 shadowPos,
                      in float2 screenUV,
                      in float4 shadowMapSize,
                      in UINT arrayIndex = 0)
{
    float o = 0.f;

    SamplerComparisonState compShadowSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    SamplerState pointShadowSampler = SamplerDescriptorHeap[ClampPointSampler];

#if defined(HARD_SHADOW)
    o = shadowMap.SampleCmpLevelZero(compShadowSampler, shadowPos.xy, shadowPos.z);
#elif defined(SOFT_SHADOW)
#if defined (SHADOW_QUALITY_LOW)
    o = Manual1x1PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler, shadowMapSize);
#elif defined (SHADOW_QUALITY_MIDDLE)
    o = Manual3x3PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler, shadowMapSize);
#elif defined (SHADOW_QUALITY_HIGH)
    o = Manual5x5PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler, shadowMapSize);
#elif defined (SHADOW_QUALITY_VERYHIGH)
    o = Manual5x5PCF(shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler, shadowMapSize);
#endif

#endif

    return o;
}

float SampleShadowPCF(in Texture2D shadowMap,
                      in float4 shadowPos,
                      in float2 screenUV,
                      in float2 screenSize,
                      in float4 shadowMapSize,
                      in float2 sobolSequence[64],
                      in float shadowRadius,
                      in UINT arrayIndex = 0)
{
    float o = 0.f;

    SamplerComparisonState compShadowSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    SamplerState pointShadowSampler = SamplerDescriptorHeap[ClampPointSampler];

#if defined(HARD_SHADOW)
    o = shadowMap.SampleCmpLevelZero(compShadowSampler, shadowPos.xy, shadowPos.z);
#elif defined(SOFT_SHADOW)
#if defined (SHADOW_QUALITY_LOW)
    o = SobolPCF(screenSize, screenUV, shadowRadius, shadowPos.xy, shadowPos.z, shadowMap,
        pointShadowSampler, shadowMapSize, sobolSequence);
#elif defined (SHADOW_QUALITY_MIDDLE)
    o = SobolPCF(screenSize, screenUV, shadowRadius, shadowPos.xy, shadowPos.z, shadowMap,
        pointShadowSampler, shadowMapSize, sobolSequence);
#elif defined (SHADOW_QUALITY_HIGH)
    o = SobolPCF(screenSize, screenUV, shadowRadius, shadowPos.xy, shadowPos.z, shadowMap,
        pointShadowSampler, shadowMapSize, sobolSequence);
#elif defined (SHADOW_QUALITY_VERYHIGH)
    o = SobolPCF(screenSize, screenUV, shadowRadius, shadowPos.xy, shadowPos.z, shadowMap,
        pointShadowSampler, shadowMapSize, sobolSequence);
#endif

#endif

    return o;
}

float SunShadowVisibility(in float3 positionWS,
                          in float2 screenUV,
                          in float4 shadowMapSize,
                          in float4x4 shadowMatrix,
                          in float2 uvOffset = 0)
{
    Texture2D shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerState shadowClampSampler = SamplerDescriptorHeap[ClampPointSampler];

    float4 shadowPos = mul(float4(positionWS, 1.f), shadowMatrix);
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;

    return SampleShadowPCF(shadowTex, shadowPos, screenUV, shadowMapSize);
}

float SunShadowVisibility(in float3 positionWS,
                          in float2 screenUV,
                          in float2 screenSize,
                          in float shadowRadius,
                          in float4 shadowMapSize,
                          in float4x4 shadowMatrix,
                          in float2 SobolSequence[64],
                          in float2 uvOffset = 0)
{
    Texture2D shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerState shadowClampSampler = SamplerDescriptorHeap[ClampPointSampler];

    float4 shadowPos = mul(float4(positionWS, 1.f), shadowMatrix);
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;

    return SampleShadowPCF(shadowTex, shadowPos, screenUV, screenSize, shadowMapSize, SobolSequence, shadowRadius);
}

#endif