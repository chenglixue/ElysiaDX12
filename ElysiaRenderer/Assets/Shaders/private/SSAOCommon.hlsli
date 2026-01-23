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
    // 计算线性深度差
    float d1 = LinearEyeDepth(DepthA, g_ZBufferParams);
    float d2 = LinearEyeDepth(DepthB, g_ZBufferParams);

    // 深度感知阈值：如果差距超过基础深度的 5%，权重迅速衰减
    float diff = abs(d1 - d2);
    float threshold = d2 * 0.003f;

    return exp(-diff / (threshold + 1e-5));
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

    // // 低分辨率纹理中周围 4 个邻居纹素（Texel）的中心点，用于后续手动双边滤波
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

float3 ReconstructNormal(float2 In)
{
    return float3(In, sqrt(1 - dot(In, In)));
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