#include "private\Common.hlsl"

#define FUSION_SETUP_THREAD_GROUP [numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
#define FUSION_DOWNSAMPLE_THREAD_GROUP [numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
#define FUSION_BLEND_THREAD_GROUP [numthreads(GROUP_SIZE, GROUP_SIZE, 1)]

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_DestSize;
    Vector4 g_SourceSize;
    UINT g_SourceTexIndex;
    UINT g_LumTexIndex;
    UINT g_WeightTexIndex;
    UINT g_CoarseLumTexIndex;
    UINT g_CoarseResultTexIndex;
    UINT g_DestLumTexIndex;
    UINT g_DestWeightTexIndex;
    UINT g_DestResultTexIndex;
    UINT g_ExposureScaleBufferIndex;
    UINT g_PreExposureScaleBufferIndex;
    UINT g_AutoExposureEnable;
    UINT g_PreExposureEnable;
    UINT g_IsCoarsest;
    float g_HighlightContrast;
    float g_ShadowContrast;
    float g_TargetLuminance;
}

// --------------------------------------------------------------------------------------- Fusion Setup ---------------------------------------------------------------------------------------
float Rec709Luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float FusionTonemap(float lum)
{
    lum = max(lum, 0.0f);
    return sqrt(lum / (1.0f + lum));
}

float FusionWeight(float value, float target)
{
    float sigma = 0.2f;
    float delta = value - target;
    return exp2(-(delta * delta) / (2.0f * sigma * sigma));
}

float OneOverPreExposure()
{
    if (g_PreExposureEnable == 0)
        return 1.0f;
    RWStructuredBuffer<float2> preExposure = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    return rcp(max(preExposure[0].r, 1e-5f));
}

float GlobalExposureScale()
{
    if (g_AutoExposureEnable == 0)
        return 1.0f;
    StructuredBuffer<float4> exposure = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
    return exposure[0].r;
}

bool IsThreadOutWorkArea(UINT2 ThreadPos, float4 AreaSize)
{
    BRANCH
    if (ThreadPos.x >= AreaSize.x || ThreadPos.y >= AreaSize.y || ThreadPos.x < 0 || ThreadPos.y < 0)
    {
        return false;
    }

    return true;
}

float3 SetupGetLuminances(float3 SceneColor)
{
    float SceneLuminance = Rec709Luminance(SceneColor);
    float SceneShadowLuminance = Rec709Luminance(SceneColor * g_ShadowContrast);
    float SceneLightLuminance = Rec709Luminance(SceneColor * g_HighlightContrast);

    return float3(SceneLuminance, SceneShadowLuminance, SceneLightLuminance);
}

float3 SetupGetTonemaps(float3 Luminances)
{
    float SceneTonemap = FusionTonemap(Luminances[0]);
    float SceneShadowTonemap = FusionTonemap(Luminances[1]);
    float SceneLightTonemap = FusionTonemap(Luminances[2]);

    return float3(SceneTonemap, SceneShadowTonemap, SceneLightTonemap);
}

float3 SetupGetWeights(float3 SceneTonemaps)
{
    float3 Weights = float3(FusionWeight(SceneTonemaps[0], g_TargetLuminance),
                            FusionWeight(SceneTonemaps[1], g_TargetLuminance),
                            FusionWeight(SceneTonemaps[2], g_TargetLuminance));
    Weights *= rcp(max(dot(Weights, 1), 1e-5));

    return Weights;
}

void SetupSaveLuminance(UINT2 SavePos, float3 Luminances, float3 Weights)
{
    RWTexture2D<float4> LuminanceTex = ResourceDescriptorHeap[g_DestLumTexIndex];
    RWTexture2D<float4> WeightTex = ResourceDescriptorHeap[g_DestWeightTexIndex];

    LuminanceTex[SavePos] = float4(Luminances, 0.f);
    WeightTex[SavePos] = float4(Weights, 0.f);
}

FUSION_SETUP_THREAD_GROUP
void FusionSetup(UINT3 ID : SV_DispatchThreadID)
{
    UINT2 ReadPos = ID.xy;
    UINT2 SavePos = ID.xy;

    BRANCH
    if (IsThreadOutWorkArea(ReadPos, g_DestSize))
    {
        return;
    }

    float3 SceneColor = LoadTexture2D(g_SourceTexIndex, ReadPos) * OneOverPreExposure() * GlobalExposureScale();
    float3 SceneLuminances = SetupGetLuminances(SceneColor);
    float3 SceneTonemaps = SetupGetTonemaps(SceneLuminances);
    float3 SceneWeights = SetupGetWeights(SceneTonemaps);

    SetupSaveLuminance(SavePos, SceneTonemaps, SceneWeights);
}

// FUSION_SETUP_THREAD_GROUP
// void FusionSetup(uint3 id : SV_DispatchThreadID)
// {
//     uint2 pixel = id.xy;
//     if (pixel.x >= (uint)g_DestSize.x || pixel.y >= (uint)g_DestSize.y)
//         return;
//
//     float3 sceneColor = LoadTexture2D(g_SourceTexIndex, pixel).rgb;
//     float lum = Rec709Luminance(sceneColor) * OneOverPreExposure() * GlobalExposureScale();
//     float baseValue = FusionTonemap(lum);
//     float highlightValue = FusionTonemap(lum * g_HighlightContrast);
//     float shadowValue = FusionTonemap(lum * g_ShadowContrast);
//
//     float3 weights = float3(
//         FusionWeight(baseValue, g_TargetLuminance),
//         FusionWeight(highlightValue, g_TargetLuminance),
//         FusionWeight(shadowValue, g_TargetLuminance));
//     weights *= rcp(max(dot(weights, 1.0f), 1e-5f));
//
//     RWTexture2D<float4> lumTex = ResourceDescriptorHeap[g_DestLumTexIndex];
//     RWTexture2D<float4> weightTex = ResourceDescriptorHeap[g_DestWeightTexIndex];
//     lumTex[pixel] = float4(baseValue, highlightValue, shadowValue, 0.0f);
//     weightTex[pixel] = float4(weights, 0.0f);
// }

// --------------------------------------------------------------------------------------- Fusion Downsample ---------------------------------------------------------------------------------------
FUSION_DOWNSAMPLE_THREAD_GROUP
void FusionDownsample(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel = id.xy;
    if (pixel.x >= (uint)g_DestSize.x || pixel.y >= (uint)g_DestSize.y)
        return;

    float3 sum = 0.0f;
    float weight = 0.0f;
    [unroll]
    for (uint y = 0; y < 2; ++y)
    {
        [unroll]
        for (uint x = 0; x < 2; ++x)
        {
            uint2 sourcePixel = pixel * 2 + uint2(x, y);
            if (sourcePixel.x < (uint)g_SourceSize.x && sourcePixel.y < (uint)g_SourceSize.y)
            {
                sum += LoadTexture2D(g_SourceTexIndex, sourcePixel).rgb;
                weight += 1.0f;
            }
        }
    }

    RWTexture2D<float4> dest = ResourceDescriptorHeap[g_DestLumTexIndex];
    dest[pixel] = float4(sum * rcp(max(weight, 1.0f)), 0.0f);
}

FUSION_BLEND_THREAD_GROUP
void FusionBlend(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel = id.xy;
    if (pixel.x >= (uint)g_DestSize.x || pixel.y >= (uint)g_DestSize.y)
        return;

    float3 weights = LoadTexture2D(g_WeightTexIndex, pixel).rgb;
    weights *= rcp(max(dot(weights, 1.0f), 1e-5f));
    float3 luminance = LoadTexture2D(g_LumTexIndex, pixel).rgb;
    float coarseResult = 0.0f;

    if (g_IsCoarsest == 0)
    {
        float2 uv = ((float2)pixel + 0.5f) * g_DestSize.zw;
        luminance -= SampleTexture2D(g_CoarseLumTexIndex, uv, ClampLinearSampler).rgb;
        coarseResult = SampleTexture2D(g_CoarseResultTexIndex, uv, ClampLinearSampler).r;
    }

    RWTexture2D<float> dest = ResourceDescriptorHeap[g_DestResultTexIndex];
    dest[pixel] = coarseResult + dot(luminance, weights);
}