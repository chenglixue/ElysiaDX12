#include "private\ShadingCommon.hlsl"
#include <private\SSAOCommon.hlsli>
#include "private\ShadowConst.hlsli"

#define GROUP_SIZE 8
static const UINT DEINTERLEAVED_DEPTH_COUNT = 4;
#define _AO_MAX_SAMPLE_COUNT 6
#define _AO_MAX_SAMPLE_STEP_COUNT 6

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_FullScreenSize;
    float4 g_DeinterleavedAOSize;
    float4 g_ImportanceBufferSize;

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
    UINT g_ReinterleaveAOTexIndex;
    UINT g_HistoryTex;

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
    float g_Sharpness_Inv;

    bool g_bDebugImportance;
    bool g_bDebugHIZMipmap;
    bool g_bImportance;

    float g_SampleImportanceThreshold;
    UINT g_AOImportanceTexIndex;
    UINT g_ActiveLayerIndex;

    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];
}

static const UINT ELYSIA_HBAO_BASE_SAMPLE_COUNT = 4;
static const UINT ELYSIA_HBAO_MAX_SAMPLE_COUNT = 6;
static const UINT ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT = 4;
static const UINT ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT = 6;
static const UINT ELYSIA_HBAO_FLEXIBLE_COUNT =
    ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT - ELYSIA_HBAO_BASE_STEP_SAMPLE_COUNT;


static const min16float4 g_BasoAOSampleArrays[] =
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

static const min16float4 g_PatternRotScaleMatrices[] =
{
    0.8670, -0.0000, -0.0000, -0.8670,
    0.8379, -0.2722, -0.2722, -0.8379,
    0.2852, -0.8778, -0.8778, -0.2852,
    0.5343, -0.7354, -0.7354, -0.5343,
    0.7241, -0.5261, -0.5261, -0.7241
};

void CalcBaseAOTap(
    const uint layerHeapIndex,
    const min16float radius,
    const float3 positionVS,
    const min16float3 normalVS,
    const float2 localUV,
    const min16float falloffCalcMulSq,
    const min16float2 sampleOffset,
    const min16float mipmapLevel,
    const min16float tapWeight,
    inout min16float obscuranceSum,
    inout min16float weightSum);
float CalcAO(UINT DepthLayerHeapIndex,
             UINT dirSampleCount,
             UINT stepSampleCount,
             float2 localUV,
             min16float radius,
             min16float screenPixelRadius,
             min16float mipmap,
             FInputParams input,
             min16float3 normalVS,
             min16float randomAngle,
             min16float jitter);
min16float4 CalcEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ);
min16float PackEdges(min16float4 edgesLRTB);
float3 NDCToViewSpace(float2 pos, float viewspaceDepth);
float ScreenSpaceToViewSpaceDepth(float screenDepth);
void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, min16float2 val);
min16float2 Elysia_Reinterleave_LoadAO(UINT index, UINT2 id);
min16float Elysia_Reinterleave_SampleAO(UINT index, float2 screenUV);
void Elysia_Reinterleave_StoreOutput(UINT2 id, min16float2 val);
float Elysia_Sample_Importance(float2 uv);
min16float4 UnpackEdges(min16float _packedVal);
float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(
    int pixel_i,
    int pixel_j,
    int sampleIndex,
    int sampleDimension);

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
    // uint layerIndex = id.z;
    uint layerIndex = g_ActiveLayerIndex;

    RWTexture2D<float2> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];
    uint layerHeapIndex = g_SourceTexIndices[layerIndex];

    float2 localScreenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;

    float4 valuesUL = GatherRedTexture2D(layerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(-1, -1));

    float pixZ = valuesUL.y;
    float eyeDepth = pixZ;

    uint offsetX = layerIndex % 2;
    uint offsetY = layerIndex * rcp(2);
    float2 fullScreenUV = (float2(id.xy * 2 + uint2(offsetX, offsetY)) + 0.5f) *
                          g_FullScreenSize.zw;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        localScreenUV,
        eyeDepth);
    inputParam.NormalWS = SampleNormalWS(fullScreenUV, ClampPointSampler);

    min16float3 normalVS = normalize(mul(inputParam.NormalWS, (float3x3)viewMatrix));

    min16float radius = g_AORadius;
    const min16float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f * rcp(projMatrix[1][1]));
    const min16float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                                 EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;
    min16float falloffCalcMulSq = -rcp(radius * radius);

    const min16float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_TargetSize.zw;
    min16float pixLookupRadiusMod = (0.85f * radius) * rcp(pixelDirRBViewspaceSizeAtCenterZ.x);

    min16float nearScreenBorder = min(min(fullScreenUV.x, 1.0 - fullScreenUV.x),
                                      min(fullScreenUV.y, 1.0 - fullScreenUV.y));
    nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
    pixLookupRadiusMod *= nearScreenBorder;

    uint pseudoRandomIndex = uint(id.y * 2 + id.x) % 5;
    min16float4 rs = g_PatternRotScaleMatrices[pseudoRandomIndex];
    min16float2x2 rotationMatrix = min16float2x2(
        rs.x * pixLookupRadiusMod,
        rs.y * pixLookupRadiusMod,
        rs.z * pixLookupRadiusMod,
        rs.w * pixLookupRadiusMod
        );

    min16float mipLevel = max(0.0f, log2(pixLookupRadiusMod) - 4.3f);

    min16float obscuranceSum = 0.0f;
    min16float weightSum = 0.0f;
    uint numberOfTaps = 5;
    [unroll]
    for (uint i = 0; i < numberOfTaps; i ++)
    {
        min16float4 kernel = g_BasoAOSampleArrays[i];
        min16float2 kernelDir = mul(kernel.xy, rotationMatrix);
        kernelDir = round(kernelDir);
        min16float2 sampleOffset = kernelDir * g_TargetSize.zw;

        min16float tapMipLevel = clamp(mipLevel + kernel.w, 0, g_HIZMaxMipmap);

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

    min16float finalObscurance = obscuranceSum * rcp(weightSum + 1e-6);
    min16float normalizedWeight = weightSum * 0.05f;

    o[id.xy].rg = min16float2(finalObscurance, normalizedWeight);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void LayeredHBAOMain(UINT3 id : SV_DispatchThreadID)
{
    // UINT layerIndex = id.z;
    UINT layerIndex = g_ActiveLayerIndex;
    UINT AOLayerHeapIndex = g_DeinterleaveAOTexIndices[layerIndex];
    UINT DepthLayerHeapIndex = g_DeinterleaveDepthTexIndices[layerIndex];

    uint offsetX = layerIndex % 2;
    uint offsetY = layerIndex * rcp(2);
    float2 localScreenUV = (float2(id.xy) + 0.5f) * g_DeinterleavedAOSize.zw;
    float2 fullResCoord = (id.xy * 2 + float2(offsetX, offsetY) + 0.5f) * g_FullScreenSize.zw;

    float4 valuesUL = GatherRedTexture2D(DepthLayerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(-1, -1));
    float4 valuesBR = GatherRedTexture2D(DepthLayerHeapIndex,
                                         localScreenUV,
                                         ClampPointSampler,
                                         int2(0, 0));
    float eyeDepth = valuesUL.y;

    FInputParams inputParam;
    inputParam.PositionVS = NDCToViewSpace(
        fullResCoord,
        eyeDepth);
    inputParam.LinearEyeDepth = eyeDepth;
    inputParam.NormalWS = SampleNormalWS(fullResCoord, ClampPointSampler);

    const min16float3 normalVS = normalize(mul(inputParam.NormalWS, viewMatrix));
    inputParam.PositionVS += normalVS * g_AOBias * eyeDepth;

    float pixZ = valuesUL.y;
    float pixLZ = valuesUL.x;
    float pixTZ = valuesUL.z;
    float pixRZ = valuesBR.z;
    float pixBZ = valuesBR.x;
    min16float4 edgeWeight = CalcEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    min16float3 nL = normalize(mul(
        SampleNormalWS(fullResCoord + min16float2(-1, 0) * g_FullScreenSize.zw, ClampPointSampler),
        (min16float3x3)viewMatrix));
    min16float3 nR = normalize(mul(
        SampleNormalWS(fullResCoord + min16float2(1, 0) * g_FullScreenSize.zw, ClampPointSampler),
        (min16float3x3)viewMatrix));
    min16float3 nT = normalize(mul(
        SampleNormalWS(fullResCoord + min16float2(0, -1) * g_FullScreenSize.zw, ClampPointSampler),
        (min16float3x3)viewMatrix));
    min16float3 nB = normalize(mul(
        SampleNormalWS(fullResCoord + min16float2(0, 1) * g_FullScreenSize.zw, ClampPointSampler),
        (min16float3x3)viewMatrix));

    const min16float dotThreshold = 0.5f;
    min16float4 normalEdgesLRTB;
    normalEdgesLRTB.x = saturate(dot(normalVS, nL) + dotThreshold);
    normalEdgesLRTB.y = saturate(dot(normalVS, nR) + dotThreshold);
    normalEdgesLRTB.z = saturate(dot(normalVS, nT) + dotThreshold);
    normalEdgesLRTB.w = saturate(dot(normalVS, nB) + dotThreshold);
    edgeWeight *= normalEdgesLRTB;
    min16float edgeFadeoutFactor = saturate((1.0 - edgeWeight.x - edgeWeight.y) * 0.35) + saturate(
                                       (1.0 - edgeWeight.z - edgeWeight.w) * 0.35);
    edgeFadeoutFactor = 1 - edgeFadeoutFactor;

    uint2 absolutePixelCoord = id.xy * 2 + uint2(offsetX, offsetY);
    float2 jitter = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(
        absolutePixelCoord.x,
        absolutePixelCoord.y,
        frameIndex,
        uint3(0, 1, 2));

    min16float randomAngle = (min16float)(jitter.x * TWO_PI);
    min16float rayJitter = (min16float)jitter.y;

    // min16float3 randomVector = SampleTexture2D(BlueNoiseTexIndex,
    //                                            fullResCoord * g_noiseScale,
    //                                            WarpPointSampler).xyz;
    // min16float temporalAngle = InterleavedGradientNoise(id.xy * 2 + UINT2(offsetX, offsetY),
    //                                                     frameIndex % 8) * TWO_PI;
    // min16float randomAngle = randomVector.x * TWO_PI + temporalAngle;

    min16float radius = g_AORadius;
    const min16float EffectSamplingRadiusNearLimitRec = rcp(radius * 1.2f / (projMatrix[1][1]));
    const min16float tooCloseLimitMod = saturate(length(inputParam.PositionVS) *
                                                 EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    radius *= tooCloseLimitMod;

    const min16float2 pixelDirRBViewspaceSizeAtCenterZ =
        inputParam.PositionVS.z * g_NDCToViewMul * g_DeinterleavedAOSize.zw;
    min16float pixLookupRadiusMod = (0.85f * radius) * rcp(pixelDirRBViewspaceSizeAtCenterZ.x);

    min16float nearScreenBorder = min(min(localScreenUV.x, 1.0 - localScreenUV.x),
                                      min(localScreenUV.y, 1.0 - localScreenUV.y));
    nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
    pixLookupRadiusMod *= nearScreenBorder;

    min16float mipLevel = max(0.0f, log2(pixLookupRadiusMod) - 4.3f);
    mipLevel /= 2;
    mipLevel = clamp(mipLevel, 0, g_HIZMaxMipmap);

    min16float importance = Elysia_Sample_Importance(
        (trunc(id.xy * rcp(2)) + 0.5f) * g_ImportanceBufferSize.zw);
    UINT dirSampleCount = lerp(ELYSIA_HBAO_BASE_SAMPLE_COUNT, ELYSIA_HBAO_MAX_SAMPLE_COUNT, importance);

    min16float baseAO = SampleTexture2D(AOLayerHeapIndex, localScreenUV, ClampPointSampler);

    min16float occlusion = 0;
    occlusion += CalcAO(DepthLayerHeapIndex,
                        dirSampleCount,
                        ELYSIA_HBAO_MAX_STEP_SAMPLE_COUNT,
                        localScreenUV,
                        radius,
                        pixLookupRadiusMod,
                        mipLevel,
                        inputParam,
                        normalVS,
                        randomAngle,
                        rayJitter);
    occlusion *= lerp(0.8, 1.2f, importance);

    min16float aoResult = occlusion * edgeFadeoutFactor;
    aoResult = saturate(1.0 - aoResult * g_AOIntensityMul * 2.f);
    aoResult = pow(abs(aoResult), g_AOIntensityPow * 2.f);
    min16float fadeRadius = max(1.f, radius);
    min16float invFadeRadius = rcp(fadeRadius);
    min16float mul = invFadeRadius;
    min16float add = -(g_AOFadeDistance - fadeRadius) * invFadeRadius;
    min16float distFade = saturate(inputParam.LinearEyeDepth * mul + add);
    aoResult = lerp(aoResult, 1.0, distFade);
    min16float outEdge = PackEdges(edgeWeight);

    Elysia_CalcAO_StoreOutput(AOLayerHeapIndex, id, min16float2(aoResult, outEdge));
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ReinterleaveMain(UINT3 id : SV_DispatchThreadID)
{
    UINT2 pixPos = id.xy;
    UINT2 readPos = pixPos / 2;

    UINT2 pixelOffset = pixPos % 2;
    UINT centerLayerIndex = pixelOffset.x + pixelOffset.y * 2;

    bool isEvenLayer = (centerLayerIndex == 0 || centerLayerIndex == 3);
    bool isComputedThisFrame = (isEvenLayer == (frameIndex % 2 == 0));

    UINT rightLayerIndex = (1 - pixelOffset.x) + pixelOffset.y * 2;
    UINT bottomLayerIndex = pixelOffset.x + (1 - pixelOffset.y) * 2;
    UINT rightBottomLayerIndex = (1 - pixelOffset.x) + (1 - pixelOffset.y) * 2;

    UINT AOCenterHeapIndex = g_DeinterleaveAOTexIndices[centerLayerIndex];
    UINT AORightHeapIndex = g_DeinterleaveAOTexIndices[rightLayerIndex];
    UINT AOBottomHeapIndex = g_DeinterleaveAOTexIndices[bottomLayerIndex];
    UINT AORightBottomHeapIndex = g_DeinterleaveAOTexIndices[rightBottomLayerIndex];

    min16float2 centerData = Elysia_Reinterleave_LoadAO(AOCenterHeapIndex, readPos);
    min16float4 edgeLRTB = UnpackEdges(centerData.g);

    float2 simpleUV = (float2(readPos) + 0.5f) * g_DeinterleavedAOSize.zw;

    // float fmx = (float)pixelOffset.x;
    // float fmy = (float)pixelOffset.y;
    // float fmxe = (edgeLRTB.y - edgeLRTB.x);
    // float fmye = (edgeLRTB.w - edgeLRTB.z);
    //
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

    min16float rightData = Elysia_Reinterleave_SampleAO(AORightHeapIndex, simpleUV);
    min16float bottomData = Elysia_Reinterleave_SampleAO(AOBottomHeapIndex, simpleUV);
    min16float rightBottomData = Elysia_Reinterleave_SampleAO(AORightBottomHeapIndex, simpleUV);

    min16float finalAO = 0.0f;
    min16float outEdge = 0.0f;
    min16float4 weight;
    weight.x = isComputedThisFrame ? 1.f : 0.f;
    weight.y = (edgeLRTB.r + edgeLRTB.g) * 0.5f;
    weight.z = (edgeLRTB.b + edgeLRTB.a) * 0.5f;
    weight.w = (weight.y + weight.z) * 0.5f;
    min16float weightSum = dot(weight, 1.f);

    finalAO = dot(min16float4(centerData.x, rightData, bottomData, rightBottomData), weight) / weightSum;
    outEdge = centerData.y;

    Elysia_Reinterleave_StoreOutput(id, min16float2(finalAO, outEdge));
}

min16float4 CalcEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
    float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return saturate((1.3 - edgesLRTB / (centerZ * 0.040)));
}

min16float PackEdges(min16float4 edgesLRTB)
{
    edgesLRTB = round(saturate(edgesLRTB) * 3.05);
    return dot(edgesLRTB, min16float4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
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
    const min16float radius,
    const float3 positionVS,
    const min16float3 normalVS,
    const float2 localUV,
    const min16float falloffCalcMulSq,
    const min16float2 sampleOffset,
    const min16float mipmapLevel,
    const min16float tapWeight,
    inout min16float obscuranceSum,
    inout min16float weightSum)
{
    float2 sampleUV = localUV + sampleOffset;
    float eyeDepth = SampleTexture2D_LOD(
        layerHeapIndex,
        sampleUV,
        ClampPointSampler,
        mipmapLevel).r;

    float3 hitPos = UVToViewSpace(sampleUV, eyeDepth);
    float3 hitDelta = hitPos - positionVS;

    min16float lengthSq = dot(hitDelta, hitDelta);
    min16float NdotD = dot(normalVS, hitDelta) * rsqrt(lengthSq + 1e-6);
    min16float falloff = max(0.0, lengthSq * falloffCalcMulSq + 1.0);
    min16float obscurance = max(0, NdotD - g_AOBias * 10) * falloff;

    min16float reduct = max(0.0, -hitDelta.z);
    reduct = saturate(reduct * rcp(-radius) + 2.0f);
    min16float weight = reduct * tapWeight;

    obscuranceSum += obscurance * weight;
    weightSum += weight;
}

float CalcAO(UINT DepthLayerHeapIndex,
             UINT dirSampleCount,
             UINT stepSampleCount,
             float2 localUV,
             min16float radius,
             min16float screenPixelRadius,
             min16float mipmap,
             FInputParams input,
             min16float3 normalVS,
             min16float randomAngle,
             min16float jitter)
{
    min16float o = 0.f;

    float2 localPixelSize = g_DeinterleavedAOSize.zw;
    float stepSizeUV = (screenPixelRadius * localPixelSize.x);

    [unroll(8)]
    for (UINT dir = 0; dir < dirSampleCount; dir ++)
    {
        min16float dirOffset = frac(jitter * (dir + 1.0f));
        min16float angle = min16float((min16float)dir + dirOffset) * rcp(float(dirSampleCount)) * TWO_PI +
                           randomAngle;

        float2 dirUV;
        sincos(angle, dirUV.y, dirUV.x);

        float2 rayDirUV = dirUV * stepSizeUV;

        min16float angleBias = g_AOBias;
        min16float topOcclusionAngle = 1e-4;
        [unroll]
        for (UINT step = 0; step < stepSampleCount; ++step)
        {
            min16float progress = (float(step) + jitter) * rcp(float(stepSampleCount));
            progress *= progress;
            float2 currentUV = localUV + rayDirUV * progress;

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

            min16float falloff = max(0, 1.0 - distSq * rcp(Pow2(radius)));

            min16float3 V_norm = v * rcp(dist + 1e-6);
            min16float sampleHorizonSin = dot(V_norm, normalVS);
            if (sampleHorizonSin > topOcclusionAngle + angleBias)
            {
                min16float diff = sampleHorizonSin - max(topOcclusionAngle, 1e-4);
                o += diff * falloff;

                topOcclusionAngle = sampleHorizonSin;
            }
        }
    }
    o *= rcp((min16float)dirSampleCount);

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

void Elysia_CalcAO_StoreOutput(UINT index, UINT2 id, min16float2 val)
{
    RWTexture2D<min16float2> o = ResourceDescriptorHeap[index];
    o[id.xy] = val;
}

min16float2 Elysia_Reinterleave_LoadAO(UINT index, UINT2 id)
{
    return LoadTexture2D(index, id);
}
min16float Elysia_Reinterleave_SampleAO(UINT index, float2 screenUV)
{
    return SampleTexture2D(index, screenUV, ClampLinearSampler).r;
}
void Elysia_Reinterleave_StoreOutput(UINT2 id, min16float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[g_ReinterleaveAOTexIndex];
    o[id.xy] = val;
}

float Elysia_Sample_Importance(float2 uv)
{
    return SampleTexture2D(g_AOImportanceTexIndex, uv, ClampLinearSampler);
}

min16float4 UnpackEdges(min16float _packedVal)
{
    uint packedVal = (uint)(_packedVal * 255.5);
    min16float4 edgesLRTB;
    edgesLRTB.x = min16float((packedVal >> 6) & 0x03) / 3.0;
    // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = min16float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = min16float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = min16float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_Sharpness_Inv);
}

float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(
    int pixel_i,
    int pixel_j,
    int sampleIndex,
    int sampleDimension)
{
    // wrap arguments
    pixel_i = pixel_i & 127;
    pixel_j = pixel_j & 127;
    sampleIndex = sampleIndex & 255;
    sampleDimension = sampleDimension & 255;

    StructuredBuffer<int> SobolBuffer = ResourceDescriptorHeap[g_SobolBufferIndex];
    StructuredBuffer<int> ScramblingTileBuffer = ResourceDescriptorHeap[g_ScramblingTileBufferIndex];
    StructuredBuffer<int> RankingTileBuffer = ResourceDescriptorHeap[g_RankingTileBufferIndex];
    // xor index based on optimized ranking
    int rankedSampleIndex = sampleIndex ^ RankingTileBuffer[sampleDimension + (pixel_i + pixel_j * 128) * 8];

    // fetch value in sequence
    int value = SobolBuffer[sampleDimension + rankedSampleIndex * 256];

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ ScramblingTileBuffer[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8];

    // convert to float and return
    float v = (0.5f + value) / 256.0f;
    return v;
}