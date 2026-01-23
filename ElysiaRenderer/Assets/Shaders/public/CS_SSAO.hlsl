#include "private\SSAOCommon.hlsli"

#define _AO_MAX_SAMPLE_COUNT 32
#define _AO_MAX_SAMPLE_STEP_COUNT 6

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

int GetRandomStep(float randomValue)
{
    // randomValue 范围 [0, 1]
    // 映射到 [2, 8] 的整数范围
    // 2 + round(randomValue * 6) 会得到 2 到 8 的整数
    return 2 + (int)round(randomValue * 6.0);
}

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void SSAO(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;

    float4 AOData = SampleTexture2D_LOD(g_HIZTextureIndex, screenUV, ClampPointSampler, g_HIZMinMipmap);
    float eyeDepth = AOData.a * Constant_Float16F_Scale;
    float3 normalWS = DecodeNormal(AOData.rgb);

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
                                          inputParam.ScreenUV * g_noiseScale, WarpPointSampler).xyz;
    float jitter = randomVector.z;
    float temporalAngle = (frameIndex % 8) * INV_FOUR_PI;

    float2 randomVec = randomVector.xy * 2.f - 1.f;
    float c = cos(temporalAngle);
    float s = sin(temporalAngle);
    float2x2 randomRotationMatrix = float2x2(
        c, -s,
        s, c
        );
    randomVec = mul(randomVec, randomRotationMatrix);
    float2x2 rotationMatrix = float2x2(
        randomVec.x, randomVec.y,
        -randomVec.y, randomVec.x
        );

    float radius = g_AORadius;

    float2 WeightAccumulator = 0.0001f; // x: 遮挡贡献，y: 样本总权重
    [unroll(_AO_MAX_SAMPLE_COUNT)]
    for (UINT i = 0; i < g_AOSampleCount; i ++)
    {
        float2 unrotatedRandomDir = g_AOSampleKernelArray[i].xy;
        float2 randomDirUV = mul(rotationMatrix, unrotatedRandomDir);
        randomDirUV *= g_TargetSize.zw * g_TargetSize.xx;

        // 计算切线角
        // 修正屏幕长宽比
        float3 randomDirVS = float3(randomDirUV.x,
                                    randomDirUV.y, 0.0f);
        float3 tangentVS = randomDirVS - normalVS * dot(randomDirVS, normalVS);

        float maxSin = dot(tangentVS, normalVS) + sin(g_AOBias);
        [unroll(_AO_MAX_SAMPLE_STEP_COUNT)]
        for (UINT step = 0; step < g_AOSampleStepCount; ++step) // 每个扇形内部步进
        {
            float stepDistance = ((float)step + jitter + 1) / g_AOSampleStepCount; // 步进距离
            stepDistance *= stepDistance;

            //  world space to uv space
            float2 offsetUV = randomDirUV * stepDistance * radius * projScale.x * 0.5f / max(
                                  inputParam.PositionVS.z, 1.0f);
            float2 vSampleUV = inputParam.ScreenUV + offsetUV;

            if (any(vSampleUV < 0) || any(vSampleUV > 1))
                continue;

            float MipLevel = ComputeMipLevel(radius, inputParam.PositionVS.z, stepDistance, jitter);
            float localEyeDepth = SampleHiZTrilinear(vSampleUV, MipLevel);
            float3 localPosVS = ComputeClipSpacePosition(vSampleUV, localEyeDepth, projMatrix);

            // 计算仰角向量
            float3 v = localPosVS - inputParam.PositionVS;
            float distSq = dot(v, v);
            float dist = sqrt(distSq);

            // 距离衰减
            float falloff = saturate(1.0 - distSq / (radius * radius));

            float3 V_norm = v / (dist + 1e-6);
            float sampleHorizonSin = dot(V_norm, normalVS);

            // 如果当前高度角超过已知最大角，更新遮挡
            if (sampleHorizonSin > maxSin)
            {
                maxSin = lerp(maxSin, sampleHorizonSin, falloff);
            }
        }

        // 累加遮挡和权重
        float occlusion = Square(1.0 - saturate(maxSin));
        WeightAccumulator.x += occlusion;
        WeightAccumulator.y += 1;
    }
    float aoResult = WeightAccumulator.x / WeightAccumulator.y;

    if (g_bLerpAO)
    {
        float4 Filtered = ComputeUpsampleContribution(g_SourceTexIndex, g_SourceSize, g_HIZTextureIndex,
                                                      g_HIZMinMipmap + 1,
                                                      inputParam.ScreenUV, inputParam.NormalWS,
                                                      inputParam.LinearEyeDepth);

        aoResult = lerp(aoResult, Filtered, g_LerpAOFactor);
    }

    float fadeRadius = max(1.f, g_AOFadeRadius);
    float invFadeRadius = 1.f / fadeRadius;
    float mul = invFadeRadius;
    float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    aoResult = lerp(aoResult, 1.0, distFade);
    aoResult = 1.0 - (1.0 - pow(abs(aoResult), g_AOIntensityPow)) * g_AOIntensityMul;

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
    // 获取相邻的两个整数层级
    uint mip0 = (uint)floor(mipmapLevel);
    uint mip1 = min(mip0 + 1, g_HIZMaxMipmap);

    // 计算两层之间的混合因子
    float t = frac(mipmapLevel);

    // 采样低层级（更精细）
    float depth0 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip0).a *
                   Constant_Float16F_Scale;

    // 采样高层级（更粗糙）
    float depth1 = SampleTexture2D_LOD(g_HIZTextureIndex, uv, ClampLinearSampler, (float)mip1).a *
                   Constant_Float16F_Scale;

    // 在两层深度之间进行线性插值
    return lerp(depth0, depth1, t);
}

float ComputeMipLevel(float radius, float sceneDepth, float step, float jitter)
{
    float screenRadius = (radius / sceneDepth) * g_TargetSize.x * step;
    float mipmapLevel = log2(screenRadius / 16.f + 1e-5);
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

    // 边界检查
    if (any(randomPosUV < 0.f) || any(randomPosUV > 1.f))
        return 0.0f;

    // 计算 Mipmap Level
    float2 offsetPixel = abs(randomPosUV - inputParam.ScreenUV) * g_TargetSize.xy;
    float screenRadius = (g_AORadius * radiusScale / inputParam.PositionVS.z) * g_TargetSize.x;
    float mipmapLevel = clamp(log2(max(offsetPixel.x, offsetPixel.y)) + jitter, (float)g_HIZMinMipmap,
                              (float)g_HIZMaxMipmap);
    mipmapLevel = clamp(log2(screenRadius / 16.f) + jitter, (float)g_HIZMinMipmap, (float)g_HIZMaxMipmap);

    float randomDepth = SampleHiZTrilinear(randomPosUV, mipmapLevel);
    float randomEyeDepth = LinearEyeDepth(randomDepth, g_ZBufferParams);

    // 遮蔽判定
    float isOccluded = step(randomEyeDepth, randomPosVS.z);

    // 范围检测：利用当前尺度的半径进行缩放
    float distDiff = abs(randomEyeDepth - inputParam.PositionVS.z);
    float currentRadius = g_AORadius * radiusScale;
    float rangeCheck = smoothstep(0.f, 1.f, currentRadius / (distDiff + 1e-5));
    float3 v = normalize(randomVec);
    float angleFactor = saturate(dot(v, normalVS) - 0.1); // 减去一个小值作为余弦 Bias

    return isOccluded * rangeCheck * angleFactor;
}

float3 GetMultiScaleBlueNoise(UINT blueNoiseTexIndex, float2 uv)
{
    // 定义多个缩放级别
#define NOISE_SCALE_LARGE   1.0    // 大尺度，低频
#define NOISE_SCALE_MEDIUM  4.0    // 中尺度，中频  
#define NOISE_SCALE_SMALL   16.0   // 小尺度，高频

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
        noiseLarge * 0.3 +  // 低频：大尺度结构
        noiseMedium * 0.4 + // 中频：细节
        noiseSmall * 0.3;   // 高频：细节

    return multiScaleNoise;
}