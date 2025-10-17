#ifndef SHADOW_COMMON_H
#define SHADOW_COMMON_H

#include "SharedCommon.hlsli"
#include "Random.hlsl"

#define N_SAMPLE 6
static float2 g_PoissonDisk[] =
{
    float2(-0.5119625f, -0.4827938f),
    float2(-0.2171264f, -0.4768726f),
    float2(-0.7552931f, -0.2426507f),
    float2(-0.7136765f, -0.4496614f),
    float2(-0.5938849f, -0.6895654f),
    float2(-0.3148003f, -0.7047654f),
    float2(-0.42215f, -0.2024607f),
    float2(-0.9466816f, -0.2014508f),
    float2(-0.8409063f, -0.03465778f),
    float2(-0.6517572f, -0.07476326f),
    float2(-0.1041822f, -0.02521214f),
    float2(-0.3042712f, -0.02195431f),
    float2(-0.5082307f, 0.1079806f),
    float2(-0.08429877f, -0.2316298f),
    float2(-0.9879128f, 0.1113683f),
    float2(-0.3859636f, 0.3363545f),
    float2(-0.1925334f, 0.1787288f),
    float2(0.003256182f, 0.138135f),
    float2(-0.8706837f, 0.3010679f),
    float2(-0.6982038f, 0.1904326f),
    float2(0.1975043f, 0.2221317f),
    float2(0.1507788f, 0.4204168f),
    float2(0.3514056f, 0.09865579f),
    float2(0.1558783f, -0.08460935f),
    float2(-0.0684978f, 0.4461993f),
    float2(0.3780522f, 0.3478679f),
    float2(0.3956799f, -0.1469177f),
    float2(0.5838975f, 0.1054943f),
    float2(0.6155105f, 0.3245716f),
    float2(0.3928624f, -0.4417621f),
    float2(0.1749884f, -0.4202175f),
    float2(0.6813727f, -0.2424808f),
    float2(-0.6707711f, 0.4912741f),
    float2(0.0005130528f, -0.8058334f),
    float2(0.02703013f, -0.6010728f),
    float2(-0.1658188f, -0.9695674f),
    float2(0.4060591f, -0.7100726f),
    float2(0.7713396f, -0.4713659f),
    float2(0.573212f, -0.51544f),
    float2(-0.3448896f, -0.9046497f),
    float2(0.1268544f, -0.9874692f),
    float2(0.7418533f, -0.6667366f),
    float2(0.3492522f, 0.5924662f),
    float2(0.5679897f, 0.5343465f),
    float2(0.5663417f, 0.7708698f),
    float2(0.7375497f, 0.6691415f),
    float2(0.2271994f, -0.6163502f),
    float2(0.2312844f, 0.8725659f),
    float2(0.4216993f, 0.9002838f),
    float2(0.4262091f, -0.9013284f),
    float2(0.2001408f, -0.808381f),
    float2(0.149394f, 0.6650763f),
    float2(-0.09640376f, 0.9843736f),
    float2(0.7682328f, -0.07273844f),
    float2(0.04146584f, 0.8313184f),
    float2(0.9705266f, -0.1143304f),
    float2(0.9670017f, 0.1293385f),
    float2(0.9015037f, -0.3306949f),
    float2(-0.5085648f, 0.7534177f),
    float2(0.9055501f, 0.3758393f),
    float2(0.7599946f, 0.1809109f),
    float2(-0.2483695f, 0.7942952f),
    float2(-0.4241052f, 0.5581087f),
    float2(-0.1020106f, 0.6724468f)
};

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

inline float ManualSobelPCF(float2 screenUV, float2 shadowPos, float lightDepth, Texture2D shadowTex, in SamplerState pcfSampler, uint CSMIndex = 0)
{
    float o = 0.f;
    
    Texture2D<float> BlueNoiseTex = ResourceDescriptorHeap[BlueNoiseTexIndex];
    SamplerComparisonState pointShadowSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    SamplerState linearSampler = SamplerDescriptorHeap[ClampLinearSampler];
    
    float2 noiseTexResolution;
    BlueNoiseTex.GetDimensions(noiseTexResolution.x, noiseTexResolution.y);
    
    uint seed = ElysiaRandomSeed(screenUV, screenSize.xy);
    float2 uv_noi = screenUV * screenSize.xy / noiseTexResolution;
    float rotateAngle = ElysiaRand(seed) * 2.0 * 3.1415926;
    rotateAngle = BlueNoiseTex.Sample(linearSampler, uv_noi * 0.5) * 2.0 * 3.1415926;
    
    float radius = 0.001;
    
    for (int i = 0; i < N_SAMPLE; ++i)
    {
        float2 offset = g_sobolSequence[i];
        offset = RotateVec2(offset, rotateAngle);
        float2 uvo = shadowPos + offset * radius;
        
        float shadowValue = shadowTex.SampleCmpLevelZero(pointShadowSampler, uvo, lightDepth).r;
        o += shadowValue;

    }
    o /= N_SAMPLE;
    
    return o;
}

float SampleShadowPCF(in Texture2D shadowMap,
                      in float4 shadowPos, in float2 screenUV, in UINT arrayIndex)
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
    o = ManualSobelPCF(screenUV, shadowPos.xy, shadowPos.z, shadowMap, pointShadowSampler);
    #endif

#endif
    
    return o;
}

float SunShadowVisibility(in float3 positionWS, in float2 screenUV, in float2 uvOffset)
{
    Texture2D shadowTex = ResourceDescriptorHeap[ShadowTexIndex];
    SamplerState shadowClampSampler = SamplerDescriptorHeap[ClampPointSampler];
    
    float4 shadowPos = mul(shadowMatrix, float4(positionWS, 1.f));
    shadowPos /= shadowPos.w;
    shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    return SampleShadowPCF(shadowTex, shadowPos, screenUV, 0);
}

#endif