#include "private\SSAOCommon.hlsli"

#define _AO_MAX_SAMPLE_COUNT 6
#define _AO_MAX_SAMPLE_STEP_COUNT 4

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_TargetSize;
    Vector4 g_SourceSize;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
    float2 g_ProjectScale;

    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
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
    UINT g_HIZTextureIndex;
    UINT g_StepMipFactor;
    bool g_bLerpAO;
    float g_LerpAOFactor;

    UINT g_RandStepTexIndex;
    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];
}

#define AO_GROUP_SIZE 8

float3 GetWorldSpaceNormalFromAOInput(float2 uv);
float SampleHiZTrilinear(float2 uv, float mipmapLevel);
float ComputeMipLevel(float radius, float sceneDepth, float step, float jitter);
float ComputeSingleAO(float3 randomVec, FInputParams inputParam, float3 normalVS, float radiusScale, float);
float3 GetMultiScaleBlueNoise(UINT blueNoiseTexIndex, float2 uv);
float ComputeMipLevel(UINT sampleID, float step);

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void HBAO(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    float4 AOData = SampleTexture2D_LOD(g_HIZTextureIndex, screenUV, ClampPointSampler, g_HIZMinMipmap);
    float eyeDepth = AOData.a * Constant_Float16F_Scale;
    float3 normalWS = AOData.rgb;

    FInputParams inputParam = (FInputParams)0;
    inputParam.PositionVS = ComputeClipSpacePosition(screenUV, eyeDepth, projMatrix);
    inputParam.NormalWS = normalWS;
    inputParam.ScreenUV = screenUV;
    inputParam.LinearEyeDepth = eyeDepth;

    // projMatrix[0][0] = 1/tan(fovX/2), projMatrix[1][1] = 1/tan(fovY/2)
    float2 projScale = float2(projMatrix[0][0], projMatrix[1][1]);

    const float3 normalVS = normalize(mul(inputParam.NormalWS, viewMatrix));
    inputParam.PositionVS += normalVS * g_AOBias * eyeDepth;

    float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                          inputParam.ScreenUV * g_noiseScale,
                                          WarpPointSampler).xyz;
    float jitter = randomVector.z;
    float temporalAngle = (frameIndex % 8) * INV_FOUR_PI;

    float2 randomVec = randomVector.xy * 2.f - 1.f;
    float c = cos(temporalAngle);
    float s = sin(temporalAngle);
    float2x2 randomRotationMatrix = float2x2(
        c,
        -s,
        s,
        c
        );
    randomVec = mul(randomVec, randomRotationMatrix);
    float2x2 rotationMatrix = float2x2(
        randomVec.x,
        randomVec.y,
        -randomVec.y,
        randomVec.x
        );

    float radius = g_AORadius;
    float pixelRadius = radius * projScale.x / max(inputParam.LinearEyeDepth, 1.f);
    pixelRadius *= 0.5f;

    float2 WeightAccumulator = 0.0001f;
    [unroll(_AO_MAX_SAMPLE_COUNT)]
    for (UINT dir = 0; dir < g_AOSampleCount; dir ++)
    {
        float2 unrotatedRandomDir = g_AOSampleKernelArray[dir].xy;
        float2 randomDirUV = mul(unrotatedRandomDir, rotationMatrix);

        float3 randomDirVS = float3(randomDirUV.x,
                                    randomDirUV.y,
                                    0.0f);
        float3 tangentVS = randomDirVS - normalVS * dot(randomDirVS, normalVS);

        float maxSin = dot(tangentVS, normalVS) + sin(g_AOBias);
        [unroll(_AO_MAX_SAMPLE_STEP_COUNT)]
        for (UINT step = 0; step < g_AOSampleStepCount; ++step)
        {
            float stepDistance = ((float)step + jitter + 1) / g_AOSampleStepCount;
            stepDistance *= stepDistance;

            //  world space to uv space
            float2 offsetUV = randomDirUV * stepDistance * pixelRadius;
            float2 vSampleUV = inputParam.ScreenUV + offsetUV;

            if (any(vSampleUV < 0) || any(vSampleUV > 1))
                continue;

            float MipLevel = ComputeMipLevel(radius, inputParam.PositionVS.z, stepDistance, jitter * 0.5f);
            float localEyeDepth = SampleTexture2D_LOD(g_HIZTextureIndex, vSampleUV, ClampLinearSampler, MipLevel).a *
                                  Constant_Float16F_Scale;
            float3 localPosVS = ComputeClipSpacePosition(vSampleUV, localEyeDepth, projMatrix);

            float3 v = localPosVS - inputParam.PositionVS;
            float distSq = dot(v, v);
            float dist = sqrt(distSq);

            float falloff = saturate(1.0 - distSq / (radius * radius));

            float3 V_norm = v / (dist + 1e-6);
            float sampleHorizonSin = dot(V_norm, normalVS);

            if (sampleHorizonSin > maxSin)
            {
                maxSin = lerp(maxSin, sampleHorizonSin, falloff);
            }
        }

        float occlusion = Square(saturate(maxSin));
        WeightAccumulator.x += occlusion;
        WeightAccumulator.y += 1;
    }
    float aoResult = WeightAccumulator.x / WeightAccumulator.y;
    aoResult = saturate(1.0 - aoResult * g_AOIntensityMul);
    aoResult = pow(abs(aoResult), g_AOIntensityPow);
    float fadeRadius = max(1.f, g_AOFadeRadius);
    float invFadeRadius = 1.f / fadeRadius;
    float mul = invFadeRadius;
    float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    aoResult = lerp(aoResult, 1.0, distFade);

    if (g_bLerpAO)
    {
        float4 Filtered = ComputeUpsampleContribution(g_SourceTexIndex,
                                                      g_SourceSize,
                                                      g_HIZTextureIndex,
                                                      g_HIZMinMipmap + 1,
                                                      inputParam.ScreenUV,
                                                      inputParam.NormalWS,
                                                      inputParam.LinearEyeDepth);

        aoResult = lerp(aoResult, Filtered, g_LerpAOFactor);
    }

    aoResult = Fast2x2Blur(float4(aoResult, inputParam.LinearEyeDepth, normalVS.xy));

    o[dispatchThreadID.xy] = float4(aoResult.rrr, 1);
}

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void HBAOPlus(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    float4 AOData = SampleTexture2D_LOD(g_HIZTextureIndex, screenUV, ClampPointSampler, g_HIZMinMipmap);
    float eyeDepth = AOData.a * Constant_Float16F_Scale;
    float3 normalWS = AOData.rgb;

    FInputParams inputParam = (FInputParams)0;
    inputParam.PositionVS = ComputeClipSpacePosition(screenUV, eyeDepth, projMatrix);
    inputParam.NormalWS = normalWS;
    inputParam.ScreenUV = screenUV;
    inputParam.LinearEyeDepth = eyeDepth;

    const float3 normalVS = normalize(mul(inputParam.NormalWS, viewMatrix));
    inputParam.PositionVS += normalVS * g_AOBias * eyeDepth;

    float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                          inputParam.ScreenUV * g_noiseScale,
                                          WarpPointSampler).xyz;
    float jitter = randomVector.z;
    float temporalAngle = frameIndex % 8 * INV_FOUR_PI;
    float randomAngle = randomVector.x * TWO_PI + temporalAngle;

    // projMatrix[0][0] = 1/tan(fovX/2), projMatrix[1][1] = 1/tan(fovY/2)
    const float2 projScale = float2(projMatrix[0][0], projMatrix[1][1]);

    const int NUM_DIRECTIONS = 4 * g_AOSampleCount;
    const int NUM_STEPS = g_AOSampleStepCount;
    float radius = g_AORadius;
    float pixelRadius = radius * projScale.x / max(inputParam.LinearEyeDepth, 1.f);
    pixelRadius *= 0.5f;

    float stepPixel = pixelRadius / (NUM_STEPS + 1);

    float occlusion = 0.f;
    [unroll(12)]
    for (UINT dir = 0; dir < NUM_DIRECTIONS; dir ++)
    {
        float angle = float(dir) / float(NUM_DIRECTIONS) * TWO_PI + randomAngle;

        float2 dirUV;
        sincos(angle, dirUV.y, dirUV.x);

        float2 deltaUV = dirUV * stepPixel;

        float rayJitter = randomVector.y;
        float2 currentUV = inputParam.ScreenUV + deltaUV * rayJitter;

        float angleBias = g_AOBias;
        float topOcclusionAngle = 1e-4;

        [unroll(_AO_MAX_SAMPLE_STEP_COUNT)]
        for (UINT step = 0; step < NUM_STEPS; ++step)
        {
            currentUV += deltaUV;

            if (any(currentUV < 0) || any(currentUV > 1))
                continue;

            float MipLevel = ComputeMipLevel(radius, inputParam.PositionVS.z, stepPixel, jitter * 0.5f);
            float localEyeDepth = SampleTexture2D_LOD(g_HIZTextureIndex, currentUV, ClampLinearSampler, MipLevel).a *
                                  Constant_Float16F_Scale;
            float3 localPosVS = ComputeClipSpacePosition(currentUV, localEyeDepth, projMatrix);

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

    if (g_bLerpAO)
    {
        float4 Filtered = ComputeUpsampleContribution(g_SourceTexIndex,
                                                      g_SourceSize,
                                                      g_HIZTextureIndex,
                                                      g_HIZMinMipmap + 1,
                                                      inputParam.ScreenUV,
                                                      inputParam.NormalWS,
                                                      inputParam.LinearEyeDepth);

        aoResult = lerp(aoResult, Filtered, g_LerpAOFactor);
    }
    aoResult = Fast2x2Blur(float4(aoResult, inputParam.LinearEyeDepth, normalVS.xy));

    o[dispatchThreadID.xy] = float4(aoResult.rrr, 1);
}

float3 GetWorldSpaceNormalFromAOInput(float2 screenUV)
{
    float4 gDepthR = GatherAlphaTexture2D(g_HIZTextureIndex, screenUV, WarpPointSampler);
    float4 gNormalR = GatherRedTexture2D(GBuffer3Index, screenUV, WarpPointSampler);
    float4 gNormalG = GatherGreenTexture2D(GBuffer3Index, screenUV, WarpPointSampler);
    float4 gNormalB = GatherBlueTexture2D(GBuffer3Index, screenUV, WarpPointSampler);

    float sceneDepth = SampleTexture2D_LOD(g_HIZTextureIndex, screenUV, WarpPointSampler, g_HIZMinMipmap).r;

    float3 weightedNormalSum = 0;
    float totalWeight = 0;
    float weights[4];
    weights[0] = ComputeDepthSimilarity(gDepthR.x, sceneDepth);
    weights[1] = ComputeDepthSimilarity(gDepthR.y, sceneDepth);
    weights[2] = ComputeDepthSimilarity(gDepthR.z, sceneDepth);
    weights[3] = ComputeDepthSimilarity(gDepthR.w, sceneDepth);

    weightedNormalSum += float3(gNormalR.x, gNormalG.x, gNormalB.x) * weights[0];
    weightedNormalSum += float3(gNormalR.y, gNormalG.y, gNormalB.y) * weights[1];
    weightedNormalSum += float3(gNormalR.z, gNormalG.z, gNormalB.z) * weights[2];
    weightedNormalSum += float3(gNormalR.w, gNormalG.w, gNormalB.w) * weights[3];
    totalWeight += weights[0] + weights[1] + weights[2] + weights[3];

    float3 avgNormal = normalize(weightedNormalSum / (totalWeight + 1e-6));
    return avgNormal;
}

float SampleHiZTrilinear(float2 uv, float mipmapLevel)
{
    uint mip0 = (uint)floor(mipmapLevel);
    uint mip1 = min(mip0 + 1, g_HIZMaxMipmap);

    float t = frac(mipmapLevel);

    float depth0 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip0).a *
                   Constant_Float16F_Scale;

    float depth1 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip1).a *
                   Constant_Float16F_Scale;

    return lerp(depth0, depth1, t);
}

float ComputeMipLevel(float radius, float sceneDepth, float step, float jitter)
{
    float screenRadius = (radius / sceneDepth) * g_TargetSize.x * step;
    float mipmapLevel = log2(screenRadius / 16.f + 1e-5) + jitter;
    mipmapLevel = clamp(mipmapLevel, g_HIZMinMipmap, g_HIZMaxMipmap);

    return mipmapLevel;
}
float ComputeMipLevel(UINT sampleID, float step)
{
    float SamplePos = (sampleID + 0.5f) / g_AOSampleCount;
    float mipmapLevel = log2(g_StepMipFactor * step * SamplePos);

    return clamp(mipmapLevel, g_HIZMinMipmap, g_HIZMaxMipmap);
}

float ComputeSingleAO(float3 randomVec, FInputParams inputParam, float3 normalVS, float radiusScale, float jitter)
{
    float3 offsetPosVS = inputParam.PositionVS;
    float4 randomPosVS = float4(randomVec + offsetPosVS, 1.f);
    float4 randomPosCS = mul(randomPosVS, projMatrix);
    float2 randomPosUV = randomPosCS.xy / randomPosCS.w * 0.5f * float2(1.f, -1.f) + 0.5f;

    if (any(randomPosUV < 0.f) || any(randomPosUV > 1.f))
        return 0.0f;

    float2 offsetPixel = abs(randomPosUV - inputParam.ScreenUV) * g_TargetSize.xy;
    float screenRadius = (g_AORadius * radiusScale / inputParam.PositionVS.z) * g_TargetSize.x;
    float mipmapLevel = clamp(log2(max(offsetPixel.x, offsetPixel.y)) + jitter,
                              (float)g_HIZMinMipmap,
                              (float)g_HIZMaxMipmap);
    mipmapLevel = clamp(log2(screenRadius / 16.f) + jitter, (float)g_HIZMinMipmap, (float)g_HIZMaxMipmap);

    float randomDepth = SampleHiZTrilinear(randomPosUV, mipmapLevel);
    float randomEyeDepth = LinearEyeDepth(randomDepth, g_ZBufferParams);

    float isOccluded = step(randomEyeDepth, randomPosVS.z);

    float distDiff = abs(randomEyeDepth - inputParam.PositionVS.z);
    float currentRadius = g_AORadius * radiusScale;
    float rangeCheck = smoothstep(0.f, 1.f, currentRadius / (distDiff + 1e-5));
    float3 v = normalize(randomVec);
    float angleFactor = saturate(dot(v, normalVS) - 0.1);

    return isOccluded * rangeCheck * angleFactor;
}

float3 GetMultiScaleBlueNoise(UINT blueNoiseTexIndex, float2 uv)
{
#define NOISE_SCALE_LARGE   1.0
#define NOISE_SCALE_MEDIUM  4.0
#define NOISE_SCALE_SMALL   16.0

    float3 noiseLarge = SampleTexture2D(BlueNoiseTexIndex,
                                        uv * NOISE_SCALE_LARGE,
                                        WarpPointSampler).xyz;

    float3 noiseMedium = SampleTexture2D(BlueNoiseTexIndex,
                                         uv * NOISE_SCALE_MEDIUM,
                                         WarpPointSampler).xyz;

    float3 noiseSmall = SampleTexture2D(BlueNoiseTexIndex,
                                        uv * NOISE_SCALE_SMALL,
                                        WarpPointSampler).xyz;

    float3 multiScaleNoise =
        noiseLarge * 0.3 +
        noiseMedium * 0.4 +
        noiseSmall * 0.3;

    return multiScaleNoise;
}