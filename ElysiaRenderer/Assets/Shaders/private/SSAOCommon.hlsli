#ifndef SSAOCommon_h
#define SSAOCommon_h

#include "ShadingCommon.hlsl"

const static float Constant_Float16F_Scale = 4096.0f * 32.0f;

// 0: not similar .. 1:very similar
float ComputeDepthSimilarity(float DepthA, float DepthB, float TweakScale)
{
    return saturate(1 - abs(DepthA - DepthB) * TweakScale);
}

// 0: not similar .. 1:very similar
float ComputeDepthSimilarity(float DepthA, float DepthB)
{
    float d1 = LinearEyeDepth(DepthA, g_ZBufferParams);
    float d2 = LinearEyeDepth(DepthB, g_ZBufferParams);

    float diff = abs(d1 - d2);
    float threshold = d2 * 0.003f;

    return exp(-diff / (threshold + 1e-5));
}

float3 ReconstructNormal(float2 In)
{
    return float3(In, sqrt(1 - dot(In, In)));
}

/// 
/// @param SourceTexIndex Down Sample AO Tex Index
/// @param SourceSize Down Sample AO Tex Size Params
/// @param HIZTexIndex HIZ Tex Index
/// @param ScreenUV 
/// @param CenterWorldNormal 
/// @return 
float4 ComputeUpsampleContribution(UINT SourceTexIndex, float4 SourceSize,
                                   UINT HIZTexIndex, float downSampleDepthMipmapLevel,
                                   float2 ScreenUV,
                                   float3 CenterWorldNormal,
                                   float EyeDepth)
{
    const int SampleCount = 4;
    float2 UV[SampleCount];

    UV[0] = ScreenUV + float2(-0.5f, 0.5f) * SourceSize.zw;
    UV[1] = ScreenUV + float2(0.5f, 0.5f) * SourceSize.zw;
    UV[2] = ScreenUV + float2(-0.5f, -0.5f) * SourceSize.zw;
    UV[3] = ScreenUV + float2(0.5f, -0.5f) * SourceSize.zw;

    // UV[0] = ScreenUV + float2(-1, -1) * SourceSize.zw;
    // UV[1] = ScreenUV + float2(0, -1) * SourceSize.zw;
    // UV[2] = ScreenUV + float2(1, -1) * SourceSize.zw;
    // UV[3] = ScreenUV + float2(-1, 0) * SourceSize.zw;
    // UV[4] = ScreenUV + float2(0, 0) * SourceSize.zw;
    // UV[5] = ScreenUV + float2(1, 0) * SourceSize.zw;
    // UV[6] = ScreenUV + float2(-1, 1) * SourceSize.zw;
    // UV[7] = ScreenUV + float2(0, 1) * SourceSize.zw;
    // UV[8] = ScreenUV + float2(1, 1) * SourceSize.zw;

    float SmallValue = 1e-5;
    float WeightSum = SmallValue;
    float4 Ret = float4(SmallValue, 0, 0, 0);

    [unroll(SampleCount)]
    for (int i = 0; i < SampleCount; ++i)
    {
        float2 SampleUV = UV[i];
        float4 DownSampleAO = SampleTexture2D(SourceTexIndex, SampleUV, WarpPointSampler);

        float4 DownSampleNormalDepth =
            SampleTexture2D_LOD(HIZTexIndex, SampleUV, WarpPointSampler, downSampleDepthMipmapLevel);
        float SampleEyeDepth = DownSampleNormalDepth.a * Constant_Float16F_Scale;
        float3 LocalWorldNormal = DecodeNormal(DownSampleNormalDepth.xyz);

        float Weight = ComputeDepthSimilarity(SampleEyeDepth, EyeDepth, 0.003);
        Weight *= saturate(dot(LocalWorldNormal, CenterWorldNormal));

        Ret += float4(DownSampleAO.rgb, 1) * Weight;
        WeightSum += Weight;
    }
    Ret /= WeightSum;

    return Ret;
}

float4 Fast2x2Blur(float4 aoData)
{
    float4 CenterPixel = aoData;
    float4 PixA = CenterPixel;
    float4 PixB = QuadReadAcrossX(CenterPixel);
    float4 PixC = QuadReadAcrossY(CenterPixel);

    float WeightA = 1.0f;
    float WeightB = 1.0f;
    float WeightC = 1.0f;

    const float NormalTweak = 4.0f;
    float3 NormalA = ReconstructNormal(PixA.zw);
    float3 NormalB = ReconstructNormal(PixB.zw);
    float3 NormalC = ReconstructNormal(PixC.zw);
    WeightB *= saturate(pow(saturate(dot(NormalA, NormalB)), NormalTweak));
    WeightC *= saturate(pow(saturate(dot(NormalA, NormalC)), NormalTweak));

    const float DepthTweak = 1;
    float InvDepth = 1.0f / PixA.y;
    WeightB *= 1 - saturate(abs(1 - PixB.y * InvDepth) * DepthTweak);
    WeightC *= 1 - saturate(abs(1 - PixC.y * InvDepth) * DepthTweak);

    float InvWeightABC = 1.0f / (WeightA + WeightB + WeightC);

    WeightA *= InvWeightABC;
    WeightB *= InvWeightABC;
    WeightC *= InvWeightABC;

    return WeightA * PixA.x + WeightB * PixB.x + WeightC * PixC.x;
}


// x = spatial direction / y = temporal direction / z = spatial offset / w = temporal offset
float4 getNoise(int2 coord, int frame)
{
    float4 noise;

    noise.x = (1.0f / 16.0f) * ((((coord.x + coord.y) & 0x3) << 2) + (coord.x & 0x3));
    noise.z = (1.0f / 4.0f) * ((coord.y - coord.x) & 0x3);

    const float rotations[] = {60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f};
    noise.y = rotations[frame % 6] * (1.0f / 360.0f);

    const float offsets[] = {0.0f, 0.5f, 0.25f, 0.75f};
    noise.w = offsets[(frame / 6) % 4];

    return noise;
}
#endif