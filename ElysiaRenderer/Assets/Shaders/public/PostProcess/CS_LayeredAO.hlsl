#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>

#define GROUP_SIZE 8
static const UINT DEINTERLEAVED_DEPTH_COUNT = 16;
#define _AO_MAX_SAMPLE_COUNT 6
#define _AO_MAX_SAMPLE_STEP_COUNT 4

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_FullScreenSize;
    UINT g_SourceTexIndex;
    UINT g_TargetTexIndex;

    UINT4 g_TargetTexIndices[DEINTERLEAVED_DEPTH_COUNT / 4];
    UINT4 g_SourceTexIndices[DEINTERLEAVED_DEPTH_COUNT / 4];

    Vector4 g_SourceSize;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
    float2 g_ProjectScale;

    UINT g_AOSampleCount;
    UINT g_AOSampleStepCount;
    float g_AORadius;
    float g_AOFadeRadius;
    float g_AOFadeDistance;
    float g_AOBias;

    float g_AOIntensityMul;
    float g_AOIntensityPow;
    float2 g_noiseScale;

    UINT g_HIZMaxMipmap;
    UINT g_HIZMinMipmap;
    //UINT g_HIZTextureIndex;
    UINT g_StepMipFactor;
    bool g_bLerpAO;
    float g_LerpAOFactor;

    UINT g_RandStepTexIndex;
    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];
}

UINT GetLayerTargetIndex(UINT i)
{
    return g_TargetTexIndices[i / 4][i % 4];
}

UINT GetLayerSourceIndex(UINT i)
{
    return g_SourceTexIndices[i / 4][i % 4];
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void DeinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    float2 screenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;
    if (screenUV.x > 1.0f || screenUV.y > 1.0f)
        return;

    // get index of 4x4
    UINT2 pixelOffset = id.xy % 4; // [0,0] to [3,3]
    UINT layerIndex = pixelOffset.x + pixelOffset.y * 4;

    // get position of pixel
    UINT2 writePos = id.xy / 4;

    float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler).r;
    float eyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams) / Constant_Float16F_Scale;

    RWTexture2D<float> o = ResourceDescriptorHeap[NonUniformResourceIndex(GetLayerTargetIndex(layerIndex))];
    o[writePos] = eyeDepth;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void LayeredHBAOMain(UINT3 id : SV_DispatchThreadID)
{
    if (id.x > g_TargetSize.x || id.y > g_TargetSize.y)
        return;

    UINT layerIndex = id.z;
    RWTexture2D<float> o = ResourceDescriptorHeap[NonUniformResourceIndex(GetLayerTargetIndex(layerIndex))];
    UINT layerHeapIndex = GetLayerSourceIndex(layerIndex);

    float2 localScreenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;
    float eyeDepth = SampleTexture2D(layerHeapIndex, localScreenUV, ClampPointSampler).r * Constant_Float16F_Scale;

    // get uv in full screen
    uint offsetX = layerIndex % 4;
    uint offsetY = layerIndex / 4;
    float2 fullScreenUV = (float2(id.xy * 4 + uint2(offsetX, offsetY)) + 0.5f) * g_FullScreenSize.zw;

    // full screen data
    FInputParams inputParam;
    inputParam.PositionVS = ComputeClipSpacePosition(fullScreenUV, eyeDepth, projMatrix);
    inputParam.LinearEyeDepth = eyeDepth;
    inputParam.NormalWS = SampleNormalWS(fullScreenUV, ClampPointSampler);

    const float3 normalVS = normalize(mul(inputParam.NormalWS, viewMatrix));
    inputParam.PositionVS += normalVS * g_AOBias * eyeDepth;

    float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                          fullScreenUV * g_noiseScale,
                                          WarpPointSampler).xyz;
    float temporalAngle = frameIndex % 8 * INV_FOUR_PI;
    float randomAngle = randomVector.x * TWO_PI + temporalAngle;

    float2 projScale = float2(projMatrix[0][0], projMatrix[1][1]);
    float radius = g_AORadius;

    const int NUM_DIRECTIONS = 4 * g_AOSampleCount;
    const int NUM_STEPS = g_AOSampleStepCount;
    float fullPixelRadius = radius * projScale.x / max(eyeDepth, 1.f) * 0.5f;
    float localPixelRadius = fullPixelRadius * 0.25f;
    float stepPixel = localPixelRadius / (g_AOSampleStepCount + 1.0f);

    float occlusion = 0.f;
    [unroll(12)]
    for (UINT dir = 0; dir < NUM_DIRECTIONS; dir ++)
    {
        float angle = float(dir) / float(NUM_DIRECTIONS) * TWO_PI + randomAngle;
        angle = float(dir) * 2.399963229728653f + randomAngle;

        float2 dirUV;
        sincos(angle, dirUV.y, dirUV.x);

        float2 deltaUV = dirUV * stepPixel;

        float rayJitter = randomVector.y;
        float2 currentUV = localScreenUV + deltaUV * rayJitter;

        float angleBias = g_AOBias;
        float topOcclusionAngle = 1e-4;
        [unroll(_AO_MAX_SAMPLE_STEP_COUNT)]
        for (UINT step = 0; step < NUM_STEPS; ++step)
        {
            currentUV += deltaUV;

            if (any(currentUV < 0) || any(currentUV > 1))
                continue;

            float2 sampleFullUV = fullScreenUV + (currentUV - localScreenUV);
            float sampleEyeDepth = SampleTexture2D(layerHeapIndex, currentUV, ClampPointSampler) *
                                   Constant_Float16F_Scale;
            float3 localPosVS = ComputeClipSpacePosition(sampleFullUV, sampleEyeDepth, projMatrix);

            float3 v = localPosVS - inputParam.PositionVS;
            float distSq = dot(v, v);
            float dist = sqrt(distSq);

            float falloff = saturate(1.0 - distSq / (radius * radius));
            float3 V_norm = v / (dist + 1e-6);
            float sampleHorizonSin = dot(V_norm, normalVS);
            if (sampleHorizonSin > topOcclusionAngle + angleBias)
            {
                float diff = sampleHorizonSin - max(topOcclusionAngle, 1e-4);
                occlusion += diff * falloff;

                topOcclusionAngle = sampleHorizonSin;
                // topOcclusionAngle = lerp(topOcclusionAngle, sampleHorizonSin, falloff);
            }
        }
    }

    float aoResult = occlusion / NUM_DIRECTIONS;
    aoResult = saturate(1.0 - aoResult * g_AOIntensityMul);
    aoResult = pow(abs(aoResult), g_AOIntensityPow);
    float fadeRadius = max(1.f, g_AOFadeRadius);
    float invFadeRadius = 1.f / fadeRadius;
    float mul = invFadeRadius;
    float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    aoResult = lerp(aoResult, 1.0, distFade);

    aoResult = Fast2x2Blur(float4(aoResult, inputParam.LinearEyeDepth, normalVS.xy));

    o[id.xy] = aoResult;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ReinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    UINT2 pixelOffset = id.xy % 4;
    UINT layerIndex = pixelOffset.x + pixelOffset.y * 4;
    UINT2 readPos = id.xy / 4;

    UINT layerHeapIndex = GetLayerSourceIndex(layerIndex);
    float ao = LoadTexture2D(layerHeapIndex, readPos);

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = ao;
}