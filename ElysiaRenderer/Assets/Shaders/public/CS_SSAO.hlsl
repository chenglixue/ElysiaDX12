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

    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];

    float2 g_noiseScale;
}

#define AO_GROUP_SIZE 8
// #define AO_PADDING 4
// #define CACHE_SIZE (AO_GROUP_SIZE + 2 * AO_PADDING)

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void SSAO(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_AOIndex];
    float3 color = LoadTexture2D(g_AOIndex, dispatchThreadID.xy);

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

    const float3 randomVector = float3(
        SampleTexture2D(BlueNoiseTexIndex,
                        inputParam.ScreenUV * g_noiseScale, WarpPointSampler).xy,
        0.f);

    const float3 normalVS = mul(inputParam.NormalWS, viewMatrix);

    // 强制让随机向量与法线垂直，得到切线
    const float3 tangent = normalize(randomVector - normalVS * dot(randomVector, normalVS));
    const float3 bitTangent = cross(normalVS, tangent);
    const float3x3 TBN = float3x3(tangent, bitTangent, normalVS);

    float AO = 0.f;
    [unroll(32)]
    for (UINT sampleIndex = 0; sampleIndex < g_AOSampleCount; ++sampleIndex)
    {
        float3 randomVec = mul(g_AOSampleKernelArray[sampleIndex].xyz, TBN) * g_AORadius;

        float4 randomPosVS = float4(randomVec, 0.f) + float4(inputParam.PositionVS, 1.f);
        float4 randomPosCS = mul(randomPosVS, projMatrix);
        float2 randomPosUV = randomPosCS.xy / randomPosCS.w * 0.5f * float2(1.f, -1.f) + 0.5f;

        float randomDepth = SampleTexture2D(OpaqueDepthIndex, randomPosUV, ClampPointSampler);
        float randomEyeDepth = LinearEyeDepth(randomDepth, g_ZBufferParams);
        float randomZ = randomPosVS.z;

        float range = step(randomEyeDepth, randomZ + g_AOBias);
        float rangeCheck = smoothstep(0.f, 1.f, g_AORadius / abs(randomEyeDepth - inputParam.PositionVS.z));

        AO += range * rangeCheck;
    }

    AO *= rcp((float)g_AOSampleCount);
    AO *= g_AOIntensityMul;

    AO = saturate(pow(AO, g_AOIntensityPow));

    color += 1 - AO;
    o[dispatchThreadID.xy] = float4(color, 1);
}