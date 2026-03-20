#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>

#define GROUP_SIZE 8
static const UINT DEINTERLEAVED_DEPTH_COUNT = 4;
#define _AO_MAX_SAMPLE_COUNT 6
#define _AO_MAX_SAMPLE_STEP_COUNT 6

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_FullScreenSize;
    float4 g_DepthNormalTexSize;
    float4 g_DeinterleavedAOSize;
    float4 g_ImportanceBufferSize;
    float4 g_UpsampleTexSize;

    Vector4 g_TargetTexIndices;
    Vector4 g_SourceTexIndices;
    Vector4 g_DeinterleaveDepthTexIndices;
    Vector4 g_DeinterLeaveNormalTexIndices;
    Vector4 g_DeinterleaveAOTexIndices;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];

    UINT g_HIZMaxMipmap;
    UINT g_SourceTexIndex;
    UINT g_TargetTexIndex;
    UINT g_ReinterleaveAOTexIndex;
    UINT g_HalfScreenTexIndex;
    UINT g_AOImportanceTexIndex;
    UINT g_AOTexIndex;

    float2 g_NDCToViewMul;
    float2 g_NDCToViewAdd;

    float2 g_DepthUnpackConsts;
    UINT g_AOSampleCount;
    UINT g_AOSampleStepCount;

    float g_AORadius;
    float g_AOFadeRadius;
    float g_AOFadeDistance;
    float g_AOBias;

    float g_AOIntensityMul;
    float g_AOIntensityPow;
    float2 g_noiseScale;

    float g_Sharpness_Inv;
    bool g_bDebugImportance;
    bool g_bDebugHIZMipmap;
    bool g_bImportance;

    float g_SampleImportanceThreshold;
    float g_BilateralSimilarityDistanceSigma;
}

static const UINT ELYSIA_HBAO_BASE_SAMPLE_COUNT = 4;
static const UINT ELYSIA_HBAO_MAX_SAMPLE_COUNT = 8;
static const UINT ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT = 2;
static const UINT ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT = 6;
static const UINT ELYSIA_HBAO_FLEXIBLE_COUNT =
    ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT - ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT;


static const float4 g_BasoAOSampleArrays[] =
{
    0.78488064, 0.56661671, 1.500000, -0.126083, 0.26022232, -0.29575172, 1.500000, -1.064030,
    0.10459357, 0.08372527, 1.110000, -2.730563, -0.68286800, 0.04963045, 1.090000, -0.498827,
    -0.13570161, -0.64190155, 1.250000, -0.532765, -0.26193795, -0.08205118, 0.670000, -1.783245,
    -0.61177456, 0.66664219, 0.710000, -0.044234, 0.43675563, 0.25119025, 0.610000, -1.167283,
    0.07884444, 0.86618668, 0.640000, -0.459002, -0.12790935, -0.29869005, 0.600000, -1.729424,
    -0.04031125, 0.02413622, 0.600000, -4.792042, 0.16201244, -0.52851415, 0.790000, -1.067055,
    -0.70991218, 0.47301072, 0.640000, -0.335236, 0.03277707, -0.22349690, 0.600000, -1.982384,
    0.68921727, 0.36800742, 0.630000, -0.266718, 0.29251814, 0.37775412, 0.610000, -1.422520,
    -0.12224089, 0.96582592, 0.600000, -0.426142, 0.11071457, -0.16131058, 0.600000, -2.165947,
    0.46562141, -0.59747696, 0.600000, -0.189760, -0.51548797, 0.11804193, 0.600000, -1.246800,
    0.89141309, -0.42090443, 0.600000, 0.028192, -0.32402530, -0.01591529, 0.600000, -1.543018,
    0.60771245, 0.41635221, 0.600000, -0.605411, 0.02379565, -0.08239821, 0.600000, -3.809046,
    0.48951152, -0.23657045, 0.600000, -1.189011, -0.17611565, -0.81696892, 0.600000, -0.513724,
    -0.33930185, -0.20732205, 0.600000, -1.698047, -0.91974425, 0.05403209, 0.600000, 0.062246,
    -0.15064627, -0.14949332, 0.600000, -1.896062, 0.53180975, -0.35210401, 0.600000, -0.758838,
    0.41487166, 0.81442589, 0.600000, -0.505648, -0.24106961, -0.32721516, 0.600000, -1.665244
};

static const float4 g_PatternRotScaleMatrices[] =
{
    0.8670, -0.0000, -0.0000, -0.8670,
    0.8379, -0.2722, -0.2722, -0.8379,
    0.2852, -0.8778, -0.8778, -0.2852,
    0.5343, -0.7354, -0.7354, -0.5343,
    0.7241, -0.5261, -0.5261, -0.7241
};

void CalcBaseAOTap(
    const uint layerHeapIndex,
    const float radius,
    const float3 positionVS,
    const float3 normalVS,
    const float2 localUV,
    const float falloffCalcMulSq,
    const float2 samplingUV,
    const float mipmapLevel,
    const float tapWeight,
    inout float obscuranceSum,
    inout float weightSum);
float CalcAO(UINT depthTexIndex,
             UINT sampleCount,
             UINT stepSampleCount,
             float2 AOScreenUV,
             float radius,
             float screenPixelRadius,
             float mipmap,
             FInputParams input,
             float3 normalVS,
             float randomAngle,
             float jitter);
float4 CalcDepthEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ);
float4 CalcNormalEdges(float3 centerN, float3 nL, float3 nR, float3 nT, float3 nB);
float PackEdges(float4 edgesLRTB);
float3 NDCToViewSpace(float2 pos, float viewspaceDepth);
float ScreenSpaceToViewSpaceDepth(float screenDepth);
void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, float2 val);
float2 Elysia_Reinterleave_LoadAO(UINT index, UINT2 id);
float Elysia_Reinterleave_SampleAO(UINT index, float2 screenUV);
void Elysia_Reinterleave_StoreOutput(UINT2 id, float2 val);
float Elysia_Sample_Importance(float2 uv);
float4 UnpackEdges(float _packedVal);
void NativeReinterleave(UINT2 id);
void UpSampleReinterleave(UINT2 id);
void UpSample(UINT2 id);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CalcBaseAO(UINT3 id : SV_DispatchThreadID)
{
    uint layerIndex = id.z;
    uint depthTexIndex = g_DeinterleaveDepthTexIndices[layerIndex];
    uint normalTexIndex = g_DeinterLeaveNormalTexIndices[layerIndex];
    uint AOTexIndex = g_DeinterleaveAOTexIndices[layerIndex];

    RWTexture2D<float2> o = ResourceDescriptorHeap[AOTexIndex];

    float2 AOScreenUV = (float2(id.xy) + 0.5f) * g_DeinterleavedAOSize.zw;
    float2 depthNormalScreenUV = (float2(id.xy) + 0.5f) * g_DepthNormalTexSize.zw;

    float4 valuesUL = GatherRedTexture2D(depthTexIndex,
                                         depthNormalScreenUV,
                                         ClampPointSampler,
                                         int2(-1, -1));
    float pixZ = valuesUL.y;
    float eyeDepth = pixZ;

    // uint offsetX = layerIndex % 2;
    // uint offsetY = layerIndex / 2;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        AOScreenUV,
        eyeDepth);
    float3 normalVS = DecodeNormal(SampleTexture2D(normalTexIndex, depthNormalScreenUV, ClampPointSampler));

    float radius = g_AORadius;
    const float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f / rcp(projMatrix[1][1]));
    const float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                            EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;
    float falloffCalcMulSq = -1.0f / (radius * radius);

    const float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_DeinterleavedAOSize.zw;
    float pixLookupRadiusMod = (0.85f * radius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    float nearScreenBorder = min(min(depthNormalScreenUV.x, 1.0 - depthNormalScreenUV.x),
                                 min(depthNormalScreenUV.y, 1.0 - depthNormalScreenUV.y));
    nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
    pixLookupRadiusMod *= nearScreenBorder;

    uint pseudoRandomIndex = uint(id.y * 2 + id.x) % 5;
    float4 rs = g_PatternRotScaleMatrices[pseudoRandomIndex];
    float2x2 rotationMatrix = float2x2(
        rs.x * pixLookupRadiusMod,
        rs.y * pixLookupRadiusMod,
        rs.z * pixLookupRadiusMod,
        rs.w * pixLookupRadiusMod
        );

    float mipLevel = max(0.0f, log2(pixLookupRadiusMod) - 4.3f);

    float obscuranceSum = 0.0f;
    float weightSum = 0.0f;
    uint numberOfTaps = 5;
    [unroll]
    for (uint i = 0; i < numberOfTaps; i ++)
    {
        float4 kernel = g_BasoAOSampleArrays[i];
        float2 kernelDir = mul(kernel.xy, rotationMatrix);
        kernelDir = round(kernelDir);
        float2 sampleOffset = kernelDir * g_DeinterleavedAOSize.zw;

        float tapMipLevel = clamp(mipLevel + kernel.w, 0, g_HIZMaxMipmap);

        CalcBaseAOTap(depthTexIndex,
                      radius,
                      inputParam.PositionVS,
                      normalVS,
                      AOScreenUV,
                      falloffCalcMulSq,
                      sampleOffset,
                      tapMipLevel,
                      kernel.z,
                      obscuranceSum,
                      weightSum);

        CalcBaseAOTap(depthTexIndex,
                      radius,
                      inputParam.PositionVS,
                      normalVS,
                      AOScreenUV,
                      falloffCalcMulSq,
                      -sampleOffset,
                      tapMipLevel,
                      kernel.z,
                      obscuranceSum,
                      weightSum);
    }

    float finalObscurance = obscuranceSum / (weightSum + 1e-6);
    float normalizedWeight = weightSum * 0.05f;

    o[id.xy].rg = float2(finalObscurance, normalizedWeight);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void LayeredHBAOMain(UINT3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    float2 readPosRounded = trunc(readPos);
    UINT layerIndex = id.z;
    UINT AOTexIndex = g_DeinterleaveAOTexIndices[layerIndex];
    UINT DepthTexIndex = g_DeinterleaveDepthTexIndices[layerIndex];
    UINT NormalTexIndex = g_DeinterLeaveNormalTexIndices[layerIndex];

    uint offsetX = layerIndex % 2;
    uint offsetY = layerIndex >> 1;
    float2 AOScreenUV = (readPosRounded + 0.5f) * g_DeinterleavedAOSize.zw;
    float2 DepthNormalScreenUV = (float2(id.xy) + 0.5f) * g_DepthNormalTexSize.zw;
    UINT2 fullScreenPos = id.xy * 4 + UINT2(offsetX, offsetY) * 2;

    float4 valuesUL = GatherRedTexture2D(DepthTexIndex,
                                         DepthNormalScreenUV,
                                         MirrorPointSampler,
                                         int2(-1, -1));
    float4 valuesBR = GatherRedTexture2D(DepthTexIndex,
                                         DepthNormalScreenUV,
                                         MirrorPointSampler,
                                         int2(0, 0));
    float eyeDepth = valuesUL.y;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        AOScreenUV,
        eyeDepth);
    inputParam.LinearEyeDepth = eyeDepth;

    const float3 normalVS = DecodeNormal(LoadTexture2D(NormalTexIndex, uint2(readPosRounded)));
    inputParam.PositionVS += normalVS * g_AOBias * eyeDepth;

    float pixZ = valuesUL.y;
    float pixLZ = valuesUL.x;
    float pixTZ = valuesUL.z;
    float pixRZ = valuesBR.z;
    float pixBZ = valuesBR.x;
    float3 nL = DecodeNormal(LoadTexture2D(NormalTexIndex, uint2(readPosRounded + int2(-1, 0))));
    float3 nR = DecodeNormal(LoadTexture2D(NormalTexIndex, uint2(readPosRounded + int2(1, 0))));
    float3 nT = DecodeNormal(LoadTexture2D(NormalTexIndex, uint2(readPosRounded + int2(0, -1))));
    float3 nB = DecodeNormal(LoadTexture2D(NormalTexIndex, uint2(readPosRounded + int2(0, 1))));

    float4 edgeWeight = CalcDepthEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    edgeWeight *= CalcNormalEdges(normalVS, nL, nR, nT, nB);
    float edgeFadeoutFactor = saturate((1.0 - edgeWeight.x - edgeWeight.y) * 0.35) + saturate(
                                  (1.0 - edgeWeight.z - edgeWeight.w) * 0.35);
    edgeFadeoutFactor = 1 - edgeFadeoutFactor;

    float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                          ((float2)fullScreenPos + 0.5f) * g_noiseScale,
                                          WarpPointSampler).xyz;
    float temporalAngle = InterleavedGradientNoise(fullScreenPos,
                                                   frameIndex % 8) * TWO_PI;
    float randomAngle = randomVector.x * TWO_PI + temporalAngle;

    float radius = g_AORadius;
    const float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f / rcp(projMatrix[1][1]));
    const float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                            EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;

    const float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_DeinterleavedAOSize.zw;
    float pixLookupRadiusMod = (0.85f * radius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    float nearScreenBorder = min(min(AOScreenUV.x, 1.0 - AOScreenUV.x),
                                 min(AOScreenUV.y, 1.0 - AOScreenUV.y));
    nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
    pixLookupRadiusMod *= nearScreenBorder;

    float mipLevel = max(0.0f, log2(pixLookupRadiusMod) - 4.3f);
    mipLevel = clamp(mipLevel, 0, g_HIZMaxMipmap);

    float importance = Elysia_Sample_Importance(
        (trunc(id.xy / 2) + 0.5f) * g_ImportanceBufferSize.zw);
    UINT dirSampleCount = ELYSIA_HBAO_MAX_SAMPLE_COUNT;

    float baseAO = SampleTexture2D(AOTexIndex, AOScreenUV, ClampPointSampler);
    float occlusion = baseAO;
    occlusion += CalcAO(DepthTexIndex,
                        dirSampleCount,
                        ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT,
                        AOScreenUV,
                        radius,
                        pixLookupRadiusMod,
                        mipLevel,
                        inputParam,
                        normalVS,
                        randomAngle,
                        randomVector.y);

    float aoResult = occlusion * edgeFadeoutFactor;
    aoResult = saturate(1.0 - aoResult * g_AOIntensityMul * 2.f);
    aoResult = pow(abs(aoResult), g_AOIntensityPow * 2.f);
    float fadeRadius = max(1.f, radius);
    float invFadeRadius = 1.f / fadeRadius;
    float mul = invFadeRadius;
    float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    // aoResult = lerp(aoResult, 1.0, distFade);
    float outEdge = PackEdges(edgeWeight);

    RWTexture2D<float3> o = ResourceDescriptorHeap[AOTexIndex];
    o[id.xy].rgb = randomVector;
    Elysia_CalcAO_StoreOutput(AOTexIndex, id, float2(aoResult, outEdge));
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ReinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    UpSampleReinterleave(id);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void UpSampleMain(UINT3 id : SV_DispatchThreadID)
{
    UpSample(id);
}

float4 CalcDepthEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
    float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return saturate(1.3 - edgesLRTB / (centerZ * 0.040));
}
float4 CalcNormalEdges(float3 centerN, float3 nL, float3 nR, float3 nT, float3 nB)
{
    const float dotThreshold = 0.5f;
    float4 normalEdgesLRTB;
    normalEdgesLRTB.x = saturate(dot(centerN, nL) + dotThreshold);
    normalEdgesLRTB.y = saturate(dot(centerN, nR) + dotThreshold);
    normalEdgesLRTB.z = saturate(dot(centerN, nT) + dotThreshold);
    normalEdgesLRTB.w = saturate(dot(centerN, nB) + dotThreshold);

    return normalEdgesLRTB;
}
float PackEdges(float4 edgesLRTB)
{
    edgesLRTB = round(saturate(edgesLRTB) * 3.05);
    return dot(edgesLRTB, float4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}
float4 UnpackEdges(float _packedVal)
{
    uint packedVal = (uint)(_packedVal * 255.5);
    float4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;
    // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_Sharpness_Inv);
}

float3 UVToViewSpace(float2 pos, float viewspaceDepth)
{
    float3 ret;
    ret.xy = (g_NDCToViewMul * pos.xy + g_NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
}
float3 NDCToViewSpace(float2 pos, float viewspaceDepth)
{
    float3 ret;

    ret.xy = (g_NDCToViewMul * pos.xy + g_NDCToViewAdd) * viewspaceDepth;

    ret.z = viewspaceDepth;

    return ret;
}
float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
    float depthLinearizeMul = g_DepthUnpackConsts.x;
    float depthLinearizeAdd = g_DepthUnpackConsts.y;

    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

void CalcBaseAOTap(
    const uint layerHeapIndex,
    const float radius,
    const float3 positionVS,
    const float3 normalVS,
    const float2 localUV,
    const float falloffCalcMulSq,
    const float2 sampleOffset,
    const float mipmapLevel,
    const float tapWeight,
    inout float obscuranceSum,
    inout float weightSum)
{
    float2 sampleUV = localUV + sampleOffset;
    float eyeDepth = SampleTexture2D_LOD(
        layerHeapIndex,
        sampleUV,
        ClampPointSampler,
        mipmapLevel).r;

    float3 hitPos = UVToViewSpace(sampleUV, eyeDepth);
    float3 hitDelta = hitPos - positionVS;

    float lengthSq = dot(hitDelta, hitDelta);
    float NdotD = dot(normalVS, hitDelta) * rsqrt(lengthSq + 1e-6);
    float falloff = max(0.0, lengthSq * falloffCalcMulSq + 1.0);
    float obscurance = max(0, NdotD - g_AOBias * 10) * falloff;

    float reduct = max(0.0, -hitDelta.z);
    reduct = saturate(reduct * rcp(-radius) + 2.0f);
    float weight = reduct * tapWeight;

    obscuranceSum += obscurance * weight;
    weightSum += weight;
}

float CalcAO(UINT depthTexIndex,
             UINT dirSampleCount,
             UINT stepSampleCount,
             float2 AOScreenUV,
             float radius,
             float screenPixelRadius,
             float mipmap,
             FInputParams input,
             float3 normalVS,
             float randomAngle,
             float jitter)
{
    float o = 0.f;

    float2 localPixelSize = g_DeinterleavedAOSize.zw;
    float stepSizeUV = (screenPixelRadius * localPixelSize.x);

    [unroll(8)]
    for (UINT dir = 0; dir < dirSampleCount; dir ++)
    {
        float angle = float(dir) / float(dirSampleCount) * TWO_PI +
                      randomAngle;

        float2 dirUV;
        sincos(angle, dirUV.y, dirUV.x);

        float2 rayDirUV = dirUV * stepSizeUV;

        float angleBias = g_AOBias;
        float topOcclusionAngle = 1e-4;
        [unroll(4)]
        for (UINT step = 0; step < stepSampleCount; ++step)
        {
            float progress = (float(step) + jitter) / float(stepSampleCount);
            progress *= progress;
            float2 currentUV = AOScreenUV + rayDirUV * progress;

            if (any(currentUV < 0) || any(currentUV > 1))
                continue;

            float sampleEyeDepth = SampleTexture2D_LOD(depthTexIndex,
                                                       currentUV,
                                                       ClampPointSampler,
                                                       mipmap);

            float3 offsetPosVS = UVToViewSpace(currentUV, sampleEyeDepth);

            float3 v = offsetPosVS - input.PositionVS;
            float distSq = dot(v, v);
            float dist = sqrt(distSq);

            if (distSq > Pow2(radius))
                continue;

            float falloff = max(0, 1.0 - distSq / Pow2(radius));

            float3 V_norm = v / (dist + 1e-6);
            float sampleHorizonSin = dot(V_norm, normalVS);
            if (sampleHorizonSin > topOcclusionAngle + angleBias)
            {
                float diff = sampleHorizonSin - max(topOcclusionAngle, 1e-4);
                o += diff * falloff;

                topOcclusionAngle = sampleHorizonSin;
            }
        }
    }
    o /= (float)dirSampleCount;

    return o;
}

void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[index];
    o[id.xy] = val;
}
float2 Elysia_Reinterleave_LoadAO(UINT index, UINT2 id)
{
    return LoadTexture2D(index, id);
}
float Elysia_Reinterleave_SampleAO(UINT index, float2 screenUV)
{
    return SampleTexture2D(index, screenUV, ClampLinearSampler).r;
}
void Elysia_Reinterleave_StoreOutput(UINT2 id, float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[g_ReinterleaveAOTexIndex];
    o[id.xy] = val;
}
float Elysia_Sample_Importance(float2 uv)
{
    return SampleTexture2D(g_AOImportanceTexIndex, uv, ClampLinearSampler);
}
void Elysia_Store_UpSample(UINT2 id, float val)
{
    RWTexture2D<float> o = ResourceDescriptorHeap[g_AOTexIndex];
    o[id.xy] = val;
}

void NativeReinterleave(UINT2 id)
{
    UINT2 pixPos = id.xy;
    UINT2 readPos = pixPos / 2;

    UINT2 pixelOffset = pixPos % 2;
    UINT centerLayerIndex = pixelOffset.x + pixelOffset.y * 2;
    UINT rightLayerIndex = (1 - pixelOffset.x) + pixelOffset.y * 2;
    UINT bottomLayerIndex = pixelOffset.x + (1 - pixelOffset.y) * 2;
    UINT rightBottomLayerIndex = (1 - pixelOffset.x) + (1 - pixelOffset.y) * 2;

    UINT AOCenterTexIndex = g_DeinterleaveAOTexIndices[centerLayerIndex];
    UINT AORightTexIndex = g_DeinterleaveAOTexIndices[rightLayerIndex];
    UINT AOBottomTexIndex = g_DeinterleaveAOTexIndices[bottomLayerIndex];
    UINT AORightBottomTexIndex = g_DeinterleaveAOTexIndices[rightBottomLayerIndex];

    float2 centerData = Elysia_Reinterleave_LoadAO(AOCenterTexIndex, readPos);
    float4 edgeLRTB = UnpackEdges(centerData.g);

    float2 simpleUV = (float2(readPos) + 0.5f) * g_DeinterleavedAOSize.zw;

    float fmx = (float)pixelOffset.x;
    float fmy = (float)pixelOffset.y;
    float fmxe = (edgeLRTB.y - edgeLRTB.x);
    float fmye = (edgeLRTB.w - edgeLRTB.z);

    // // Horizontal Neighbor UV
    // float2 uvH = (float2(pixPos) + float2(fmx + fmxe - 0.5, 0.5 - fmy)) * 0.5 *
    //              g_DeinterleavedAOSize.zw;
    //
    // // Vertical Neighbor UV
    // float2 uvV = (float2(pixPos) + float2(0.5 - fmx, fmy - 0.5 + fmye)) * 0.5 *
    //              g_DeinterleavedAOSize.zw;
    //
    // // Diagonal Neighbor UV
    // float2 uvD = (float2(pixPos) + float2(fmx - 0.5 + fmxe, fmy - 0.5 + fmye)) * 0.5 *
    //              g_DeinterleavedAOSize.zw;

    float rightData = Elysia_Reinterleave_SampleAO(AORightTexIndex, simpleUV);
    float bottomData = Elysia_Reinterleave_SampleAO(AOBottomTexIndex, simpleUV);
    float rightBottomData = Elysia_Reinterleave_SampleAO(AORightBottomTexIndex, simpleUV);

    float4 weight;
    weight.x = 1.f;
    weight.y = (edgeLRTB.r + edgeLRTB.g) * 0.5f;
    weight.z = (edgeLRTB.b + edgeLRTB.a) * 0.5f;
    weight.w = (weight.y + weight.z) * 0.5f;

    float weightSum = dot(weight, 1.f);

    float finalAO = dot(float4(centerData.x, rightData, bottomData, rightBottomData),
                        weight) / weightSum;
    Elysia_Reinterleave_StoreOutput(id, float2(finalAO, centerData.y));
}
void UpSampleReinterleave(UINT2 id)
{
    UINT2 pixPos = id.xy;
    if (pixPos.x >= g_UpsampleTexSize.x || pixPos.y >= g_UpsampleTexSize.y)
        return;

    // index of 2x2 block
    UINT2 layerIndex = pixPos % 2;
    int centerLayerIndex = layerIndex.x + layerIndex.y * 2;
    UINT horizionLayerIndex = (1 - layerIndex.x) + layerIndex.y * 2;
    UINT verticalLayerIndex = layerIndex.x + (1 - layerIndex.y) * 2;
    UINT diagonalLayerIndex = (1 - layerIndex.x) + (1 - layerIndex.y) * 2;
    UINT AOCenterTexIndex = g_DeinterleaveAOTexIndices[centerLayerIndex];
    UINT AOHorizionTexIndex = g_DeinterleaveAOTexIndices[horizionLayerIndex];
    UINT AOverticalTexIndex = g_DeinterleaveAOTexIndices[verticalLayerIndex];
    UINT AODiagonalTexIndex = g_DeinterleaveAOTexIndices[diagonalLayerIndex];

    UINT2 AOPos = pixPos / 2;
    float2 centerData = Elysia_Reinterleave_LoadAO(AOCenterTexIndex, AOPos);
    float4 edgeLRTB = UnpackEdges(centerData.g);

    float2 fLayerIndex = (float2)layerIndex;
    // calc edge, if edge bigger, offset to this edge dir; if edge smaller, offset to Opposite dir
    float horizionEdgeDiff = (edgeLRTB.y - edgeLRTB.x);
    float verticalEdgeDiff = (edgeLRTB.w - edgeLRTB.z);

    // bilinear filiter
    float2 fReadPos = float2(pixPos);
    // Horizontal Neighbor UV
    float2 uvH = (fReadPos + float2(fLayerIndex.x + horizionEdgeDiff - 0.5f, 0.5f - fLayerIndex.y)) * 0.5 *
                 g_DeinterleavedAOSize.zw;
    float horizionData = Elysia_Reinterleave_SampleAO(AOHorizionTexIndex, uvH);

    // Vertical Neighbor UV
    float2 uvV = (fReadPos + float2(0.5 - fLayerIndex.x, fLayerIndex.y - 0.5 + verticalEdgeDiff)) * 0.5 *
                 g_DeinterleavedAOSize.zw;
    float verticalData = Elysia_Reinterleave_SampleAO(AOverticalTexIndex, uvV);

    // Diagonal Neighbor UV
    float2 uvD = (fReadPos + float2(fLayerIndex.x - 0.5 + horizionEdgeDiff, fLayerIndex.y - 0.5 + verticalEdgeDiff)) *
                 0.5 *
                 g_DeinterleavedAOSize.zw;
    float diagonalData = Elysia_Reinterleave_SampleAO(AODiagonalTexIndex, uvD);

    float4 weight;
    weight.x = 1.f;
    weight.y = (edgeLRTB.r + edgeLRTB.g) * 0.5f;
    weight.z = (edgeLRTB.b + edgeLRTB.a) * 0.5f;
    weight.w = (weight.y + weight.z) * 0.5f;

    float weightSum = dot(weight, 1.f);
    float finalAO = dot(float4(centerData.x, horizionData, verticalData, diagonalData),
                        weight) / weightSum;

    UINT2 depthReadPos = pixPos >> 1;
    UINT depthTexIndex = g_DeinterleaveDepthTexIndices[centerLayerIndex];
    float depth = LoadTexture2D(depthTexIndex, depthReadPos);

    Elysia_Reinterleave_StoreOutput(id, float2(finalAO, depth));
}

void UpSample(UINT2 id)
{
    UINT2 readPos = id.xy;
    if (readPos.x >= g_FullScreenSize.x || readPos.y >= g_FullScreenSize.y)
        return;

    float2 uv = (float2(readPos) + 0.5f) * g_FullScreenSize.zw;

    float fullScreenDepth = LoadTexture2D(OpaqueDepthIndex, readPos);
    fullScreenDepth = ScreenSpaceToViewSpaceDepth(fullScreenDepth);
    float4 halfScreenDepth = GatherGreenTexture2D(g_HalfScreenTexIndex, uv, ClampPointSampler);
    float4 halfScreenAO = GatherRedTexture2D(g_HalfScreenTexIndex, uv, ClampPointSampler);

    float2 fracPos = frac((float2(readPos) + 0.5f) * 0.5f);
    float4 spatialWeights;
    spatialWeights.x = (1.f - fracPos.x) * fracPos.y;
    spatialWeights.y = fracPos.x * fracPos.y;
    spatialWeights.z = fracPos.x * (1.f - fracPos.y);
    spatialWeights.w = (1.f - fracPos.x) * (1.f - fracPos.y);

    float4 depthDiff = abs(fullScreenDepth.xxxx - halfScreenDepth);
    float4 rangeWeights = exp(-depthDiff / g_BilateralSimilarityDistanceSigma);

    float4 totalWeights = spatialWeights * rangeWeights;
    float weightSum = dot(totalWeights, 1.f) + 1e-4f;
    float finalAO = dot(halfScreenAO, totalWeights) / weightSum;
    Elysia_Store_UpSample(readPos, finalAO);
}