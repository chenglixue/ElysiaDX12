#include "private\ShadingCommon.hlsl"

#define _AO_MAX_SAMPLE_COUNT 32

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_TargetSize;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    UINT g_AOSampleCount;
    float g_AORadius;
    float g_AOBias;
    float g_AOIntensityMul;
    float g_AOIntensityPow;

    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT + 1];

    float2 g_noiseScale;
    UINT g_HIZMaxMipmap;
    UINT g_HIZTextureIndex;
}

#define AO_GROUP_SIZE 8

float SampleHiZTrilinear(float2 uv, float mipmapLevel);
float ComputeSingleAO(float3 randomVec, FInputParams inputParam, float3 normalVS, float radiusScale, float);

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void SSAO(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_AOIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);
    float3 positionWS = ComputeWorldSpacePosition(screenUV, GBufferData.Depth, viewProjMatrix_I);
    FInputParams inputParam = (FInputParams)0;
    inputParam.PositionWS = positionWS;
    inputParam.PositionVS = mul(float4(positionWS, 1.f), viewMatrix);
    inputParam.PixelPos = screenUV * g_TargetSize.xy;
    inputParam.ScreenUV = screenUV;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    inputParam.Linear01Depth = Linear01Depth(GBufferData.Depth, g_ZBufferParams);
    inputParam.LinearEyeDepth = LinearEyeDepth(GBufferData.Depth, g_ZBufferParams);

    const float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                                inputParam.ScreenUV * g_noiseScale, WarpPointSampler).xyz;

    const float3 normalVS = mul(inputParam.NormalWS, viewMatrix);

    // 强制让随机向量与法线垂直，得到切线
    float3 tangent = normalize(randomVector - normalVS * dot(randomVector, normalVS));
    const float3 bitTangent = cross(normalVS, tangent);
    float3x3 TBN = float3x3(tangent, bitTangent, normalVS);

    float AO_Small = 0.0f;
    float AO_Large = 0.0f;
    [unroll(16)]
    for (UINT sampleIndex = 0; sampleIndex < g_AOSampleCount; ++sampleIndex)
    {
        float3 randomVec = mul(g_AOSampleKernelArray[sampleIndex], TBN);

        float3 vecLarge = randomVec * g_AORadius;
        AO_Large += ComputeSingleAO(vecLarge, inputParam, normalVS, 1.0, randomVector * 0.5f);

        float3 vecSmall = randomVec * (g_AORadius * 0.2f);
        AO_Small += ComputeSingleAO(vecSmall, inputParam, normalVS, 0.2, randomVector * 0.5f);
    }

    AO_Large /= (float)g_AOSampleCount;
    AO_Small /= (float)g_AOSampleCount;

    float combinedAO = saturate(AO_Large * 0.3f + AO_Small * 0.7f);
    combinedAO *= g_AOIntensityMul;

    combinedAO = saturate(pow(combinedAO, g_AOIntensityPow));

    float3 result = 1 - combinedAO;
    o[dispatchThreadID.xy] = float4(result, 1);
}

float SampleHiZTrilinear(float2 uv, float mipmapLevel)
{
    // 获取相邻的两个整数层级
    uint mip0 = (uint)floor(mipmapLevel);
    uint mip1 = min(mip0 + 1, g_HIZMaxMipmap);

    // 计算两层之间的混合因子
    float t = frac(mipmapLevel);

    // 采样低层级（更精细）
    float depth0 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip0);

    // 采样高层级（更粗糙）
    float depth1 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip1);

    // 在两层深度之间进行线性插值
    return lerp(depth0, depth1, t);
}

float ComputeSingleAO(float3 randomVec, FInputParams inputParam, float3 normalVS, float radiusScale, float jitter)
{
    float4 randomPosVS = float4(randomVec, 0.f) + float4(inputParam.PositionVS + normalVS * g_AOBias, 1.f);
    float4 randomPosCS = mul(randomPosVS, projMatrix);
    float2 randomPosUV = randomPosCS.xy / randomPosCS.w * 0.5f * float2(1.f, -1.f) + 0.5f;

    // 边界检查
    if (any(randomPosUV < 0.f) || any(randomPosUV > 1.f))
        return 0.0f;

    // 计算 Mipmap Level
    float2 offsetPixel = abs(randomPosUV - inputParam.ScreenUV) * g_TargetSize.xy;
    float mipmapLevel = clamp(log2(max(offsetPixel.x, offsetPixel.y)) + jitter, 0.f, (float)g_HIZMaxMipmap);

    float randomDepth = SampleHiZTrilinear(randomPosUV, mipmapLevel);
    float randomEyeDepth = LinearEyeDepth(randomDepth, g_ZBufferParams);

    // 遮蔽判定
    float range = step(randomEyeDepth, randomPosVS.z + g_AOBias);

    // 范围检测：利用当前尺度的半径进行缩放
    float currentRadius = g_AORadius * radiusScale;
    float rangeCheck = smoothstep(0.f, 1.f, currentRadius / (abs(randomEyeDepth - inputParam.PositionVS.z) + 1e-5));
    rangeCheck = Pow2(rangeCheck);

    return range * rangeCheck;
}