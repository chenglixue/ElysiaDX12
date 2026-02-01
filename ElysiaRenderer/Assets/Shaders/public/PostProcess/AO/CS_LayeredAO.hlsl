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
    float4 g_DeinterleavedAOSize;

    Vector4 g_TargetTexIndices;
    Vector4 g_SourceTexIndices;
    Vector4 g_DeinterleaveDepthTexIndices;
    Vector4 g_DeinterleaveAOTexIndices;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    UINT g_SourceTexIndex;
    UINT g_TargetTexIndex;

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

    UINT g_HIZMaxMipmap;
    UINT g_HIZTextureIndex;

    bool g_bDebugImportance;
    bool g_bDebugHIZMipmap;
    bool g_bImportance;

    float g_SampleImportanceThreshold;
    UINT g_AOImportanceTexIndex;

    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];
}

static const UINT ELYSIA_HBAO_BASE_SAMPLE_COUNT = 1;
static const UINT ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT = 2;

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
float CalcAO(UINT DepthLayerHeapIndex,
             UINT sampleCount,
             UINT stepSampleCount,
             float2 localUV,
             float radius,
             float screenPixelRadius,
             float mipmap,
             FInputParams input,
             float3 normalVS,
             float randomAngle,
             float jitter);
float4 CalcEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ);
float PackEdges(float4 edgesLRTB);
float3 NDCToViewSpace(float2 pos, float viewspaceDepth);
float ScreenSpaceToViewSpaceDepth(float screenDepth);
void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, float2 val);
float Elysia_Sample_Importance(float2 uv);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void DeinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    float2 screenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;

    UINT2 pixelOffset = id.xy % 2;
    UINT layerIndex = pixelOffset.x + pixelOffset.y * 2;

    UINT2 writePos = id.xy / 2;

    float eyeDepth = SampleTexture2D(g_SourceTexIndex,
                                     screenUV,
                                     ClampPointSampler).r;
    eyeDepth = ScreenSpaceToViewSpaceDepth(eyeDepth);

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];
    o[writePos] = eyeDepth;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CalcBaseAO(UINT3 id : SV_DispatchThreadID)
{
    uint layerIndex = id.z;
    RWTexture2D<float2> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];
    uint layerHeapIndex = g_SourceTexIndices[layerIndex];

    float2 localScreenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;

    float4 valuesUL = GatherRedTexture2D(layerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(-1, -1));
    // float4 valuesBR = GatherRedTexture2D(layerHeapIndex,
    //                                      localScreenUV,
    //                                      ClampPointSampler,
    //                                      int2(0, 0));

    float pixZ = valuesUL.y;
    // float pixLZ = valuesUL.x;
    // float pixTZ = valuesUL.z;
    // float pixRZ = valuesBR.z;
    // float pixBZ = valuesBR.x;

    //float4 edgeWeight = CalcEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    float eyeDepth = pixZ;

    uint offsetX = layerIndex % 2;
    uint offsetY = layerIndex / 2;
    float2 fullScreenUV = (float2(id.xy * 2 + uint2(offsetX, offsetY)) + 0.5f) *
                          g_FullScreenSize.zw;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        localScreenUV,
        eyeDepth);
    inputParam.NormalWS = SampleNormalWS(fullScreenUV, ClampPointSampler);

    float3 normalVS = normalize(mul(inputParam.NormalWS, (float3x3)viewMatrix));

    // float3 nL = normalize(mul(
    //     SampleNormalWS(fullScreenUV + float2(-1, 0) * g_FullScreenSize.zw, ClampPointSampler),
    //     (float3x3)viewMatrix));
    // float3 nR = normalize(mul(
    //     SampleNormalWS(fullScreenUV + float2(1, 0) * g_FullScreenSize.zw, ClampPointSampler),
    //     (float3x3)viewMatrix));
    // float3 nT = normalize(mul(
    //     SampleNormalWS(fullScreenUV + float2(0, -1) * g_FullScreenSize.zw, ClampPointSampler),
    //     (float3x3)viewMatrix));
    // float3 nB = normalize(mul(
    //     SampleNormalWS(fullScreenUV + float2(0, 1) * g_FullScreenSize.zw, ClampPointSampler),
    //     (float3x3)viewMatrix));

    //const float dotThreshold = 0.5f;

    // float4 normalEdgesLRTB;
    // normalEdgesLRTB.x = saturate(dot(normalVS, nL) + dotThreshold);
    // normalEdgesLRTB.y = saturate(dot(normalVS, nR) + dotThreshold);
    // normalEdgesLRTB.z = saturate(dot(normalVS, nT) + dotThreshold);
    // normalEdgesLRTB.w = saturate(dot(normalVS, nB) + dotThreshold);
    // edgeWeight *= normalEdgesLRTB;

    float radius = g_AORadius;
    const float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f / rcp(projMatrix[1][1]));
    const float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                            EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;
    float falloffCalcMulSq = -1.0f / (radius * radius);

    const float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_TargetSize.zw;
    float pixLookupRadiusMod = (0.85f * radius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    float nearScreenBorder = min(min(fullScreenUV.x, 1.0 - fullScreenUV.x),
                                 min(fullScreenUV.y, 1.0 - fullScreenUV.y));
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
        float2 sampleOffset = kernelDir * g_TargetSize.zw;

        float tapMipLevel = clamp(mipLevel + kernel.w, 0, g_HIZMaxMipmap);

        CalcBaseAOTap(layerHeapIndex,
                      radius,
                      inputParam.PositionVS,
                      normalVS,
                      localScreenUV,
                      falloffCalcMulSq,
                      sampleOffset,
                      tapMipLevel,
                      kernel.z,
                      obscuranceSum,
                      weightSum);

        CalcBaseAOTap(layerHeapIndex,
                      radius,
                      inputParam.PositionVS,
                      normalVS,
                      localScreenUV,
                      falloffCalcMulSq,
                      -sampleOffset,
                      tapMipLevel,
                      kernel.z,
                      obscuranceSum,
                      weightSum);
    }

    float finalObscurance = obscuranceSum / (weightSum + 1e-6);
    float normalizedWeight = weightSum * 0.05f;
    // normalizedWeight *= PackEdges(edgeWeight);

    o[id.xy].rg = float2(finalObscurance, normalizedWeight);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void LayeredHBAOMain(UINT3 id : SV_DispatchThreadID)
{
    UINT layerIndex = id.z;
    UINT AOLayerHeapIndex = g_DeinterleaveAOTexIndices[layerIndex];
    UINT DepthLayerHeapIndex = g_DeinterleaveDepthTexIndices[layerIndex];
    float2 localScreenUV = (float2(id.xy) + 0.5f) * g_DeinterleavedAOSize.zw;

    float4 valuesUL = GatherRedTexture2D(DepthLayerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(-1, -1));
    float4 valuesBR = GatherRedTexture2D(DepthLayerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(0, 0));
    float eyeDepth = valuesUL.y;

    float importance = Elysia_Sample_Importance(localScreenUV);
    UINT fixDirSampleCount = max(0, g_AOSampleCount - ELYSIA_HBAO_BASE_SAMPLE_COUNT);
    UINT dirSampleCount = ELYSIA_HBAO_BASE_SAMPLE_COUNT + round(fixDirSampleCount * importance);
    dirSampleCount = max(dirSampleCount, ELYSIA_HBAO_BASE_SAMPLE_COUNT) * 4;
    UINT stepSampleCount = ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT +
                           round((g_AOSampleStepCount - ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT)
                                 * importance);
    stepSampleCount = max(stepSampleCount, ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT);

    uint offsetX = layerIndex % 2;
    uint offsetY = layerIndex / 2;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        localScreenUV,
        eyeDepth);
    inputParam.LinearEyeDepth = eyeDepth;
    inputParam.NormalWS = SampleNormalWS(localScreenUV, ClampPointSampler);

    const float3 normalVS = normalize(mul(inputParam.NormalWS, viewMatrix));

    float pixZ = valuesUL.y;
    float pixLZ = valuesUL.x;
    float pixTZ = valuesUL.z;
    float pixRZ = valuesBR.z;
    float pixBZ = valuesBR.x;
    float4 edgeWeight = CalcEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    float3 nL = normalize(mul(
        SampleNormalWS(localScreenUV + float2(-1, 0) * g_FullScreenSize.zw, ClampPointSampler),
        (float3x3)viewMatrix));
    float3 nR = normalize(mul(
        SampleNormalWS(localScreenUV + float2(1, 0) * g_FullScreenSize.zw, ClampPointSampler),
        (float3x3)viewMatrix));
    float3 nT = normalize(mul(
        SampleNormalWS(localScreenUV + float2(0, -1) * g_FullScreenSize.zw, ClampPointSampler),
        (float3x3)viewMatrix));
    float3 nB = normalize(mul(
        SampleNormalWS(localScreenUV + float2(0, 1) * g_FullScreenSize.zw, ClampPointSampler),
        (float3x3)viewMatrix));

    const float dotThreshold = 0.5f;
    float4 normalEdgesLRTB;
    normalEdgesLRTB.x = saturate(dot(normalVS, nL) + dotThreshold);
    normalEdgesLRTB.y = saturate(dot(normalVS, nR) + dotThreshold);
    normalEdgesLRTB.z = saturate(dot(normalVS, nT) + dotThreshold);
    normalEdgesLRTB.w = saturate(dot(normalVS, nB) + dotThreshold);
    edgeWeight *= normalEdgesLRTB;
    float edgeFadeoutFactor = saturate((1.0 - edgeWeight.x - edgeWeight.y) * 0.35) + saturate(
                                  (1.0 - edgeWeight.z - edgeWeight.w) * 0.35);
    edgeFadeoutFactor = 1 - edgeFadeoutFactor;

    float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
                                          localScreenUV * g_noiseScale,
                                          WarpPointSampler).xyz;
    float temporalAngle = frameIndex % 8 * INV_FOUR_PI;
    float randomAngle = randomVector.x * TWO_PI + temporalAngle;

    float radius = g_AORadius;
    const float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f / rcp(projMatrix[1][1]));
    const float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                            EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;

    const float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_DeinterleavedAOSize.zw;
    float pixLookupRadiusMod = (0.85f * radius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    float nearScreenBorder = min(min(localScreenUV.x, 1.0 - localScreenUV.x),
                                 min(localScreenUV.y, 1.0 - localScreenUV.y));
    nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
    pixLookupRadiusMod *= nearScreenBorder;

    float mipLevel = max(0.0f, log2(pixLookupRadiusMod) - 4.3f);
    mipLevel = clamp(mipLevel, 0, g_HIZMaxMipmap);

    float occlusion = 0.f;
    occlusion += CalcAO(DepthLayerHeapIndex,
                        dirSampleCount,
                        stepSampleCount,
                        localScreenUV,
                        radius,
                        pixLookupRadiusMod,
                        mipLevel,
                        inputParam,
                        normalVS,
                        randomAngle,
                        randomVector.y);

    float aoResult = occlusion * edgeFadeoutFactor;
    aoResult = saturate(1.0 - aoResult * g_AOIntensityMul);
    aoResult = pow(abs(aoResult), g_AOIntensityPow);
    float fadeRadius = max(1.f, radius);
    float invFadeRadius = 1.f / fadeRadius;
    float mul = invFadeRadius;
    float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    aoResult = lerp(aoResult, 1.0, distFade);
    float outEdge = PackEdges(edgeWeight);

    Elysia_CalcAO_StoreOutput(AOLayerHeapIndex, id, float2(aoResult, outEdge));
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ReinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    uint2 quarterCoord = id.xy / 4;
    UINT2 pixelOffset = id.xy % 2;
    UINT layerIndex = pixelOffset.x + pixelOffset.y * 2;
    UINT2 readPos = id.xy / 2;

    float sample = LoadTexture2D(g_DeinterleaveDepthTexIndices[layerIndex], readPos);

    // float2 fullScreenUV = (float2(id.xy) + 0.5f) * g_FullScreenSize.zw;
    // float rawDepth = SampleTexture2D(OpaqueDepthIndex, fullScreenUV, ClampPointSampler).r;
    // float currentDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);
    //
    // float sumAO = 0.0f;
    // float sumWeight = 0.0f;
    //
    // [unroll]
    // for (uint i = 0; i < 4; ++i)
    // {
    //     uint depthSliceIndex = g_DeinterleaveDepthTexIndices[i];
    //     Texture2D<float> depthSlice = ResourceDescriptorHeap[depthSliceIndex];
    //     float sliceDepth = depthSlice[quarterCoord].r *
    //                        Constant_Float16F_Scale;
    //
    //     uint aoSliceIndex = g_DeinterleaveAOTexIndices[i];
    //     Texture2D<float> AOSlice = ResourceDescriptorHeap[aoSliceIndex];
    //     float sliceAO = AOSlice[quarterCoord].r;
    //
    //     float depthDiff = abs(currentDepth - sliceDepth);
    //     float weight = 1.0f / (depthDiff + 0.001f);
    //
    //     sumAO += sliceAO * weight;
    //     sumWeight += weight;
    // }
    // float blendedAO = sumAO / (sumWeight + 1e-6f);

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = sample;
}

float4 CalcEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
    float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return saturate((1.3 - edgesLRTB / (centerZ * 0.040)));
}

float PackEdges(float4 edgesLRTB)
{
    edgesLRTB = round(saturate(edgesLRTB) * 3.05);
    return dot(edgesLRTB, float4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

float3 UVToViewSpace(float2 pos, float viewspaceDepth)
{
    float3 ret;
    ret.xy = (g_NDCToViewMul * pos.xy + g_NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
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

float CalcAO(UINT DepthLayerHeapIndex,
             UINT dirSampleCount,
             UINT stepSampleCount,
             float2 localUV,
             float radius,
             float screenPixelRadius,
             float mipmap,
             FInputParams input,
             float3 normalVS,
             float randomAngle,
             float jitter)
{
    float o = 0.f;

    if (dirSampleCount == 0)
        return 0.0f;
    float2 localPixelSize = g_DeinterleavedAOSize.zw;
    float stepSizeUV = (screenPixelRadius * localPixelSize.x) / (stepSampleCount + 1.0f);

    [loop]
    for (UINT dir = 0; dir < dirSampleCount; dir ++)
    {
        float angle = float(dir) / float(dirSampleCount) * TWO_PI +
                      randomAngle;

        float2 dirUV;
        sincos(angle, dirUV.y, dirUV.x);

        float2 deltaUV = dirUV * stepSizeUV;

        float rayJitter = jitter;
        float2 currentUV = localUV + deltaUV * rayJitter;

        float angleBias = g_AOBias;
        float topOcclusionAngle = 1e-4;
        [loop]
        for (UINT step = 0; step < stepSampleCount; ++step)
        {
            currentUV += deltaUV;

            if (any(currentUV < 0) || any(currentUV > 1))
                continue;

            float sampleEyeDepth = SampleTexture2D_LOD(DepthLayerHeapIndex,
                                                       currentUV,
                                                       ClampPointSampler,
                                                       mipmap);

            float3 localPosVS = UVToViewSpace(currentUV, sampleEyeDepth);

            float3 v = localPosVS - input.PositionVS;
            float distSq = dot(v, v);
            float dist = sqrt(distSq);

            if (distSq > Pow2(radius))
                continue;

            float falloff = saturate(1.0 - distSq / Pow2(radius));

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
    o /= dirSampleCount;

    return o;
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

void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[index];
    o[id.xy] = val;
}

float Elysia_Sample_Importance(float2 uv)
{
    return SampleTexture2D(g_AOImportanceTexIndex, uv, ClampPointSampler);
}