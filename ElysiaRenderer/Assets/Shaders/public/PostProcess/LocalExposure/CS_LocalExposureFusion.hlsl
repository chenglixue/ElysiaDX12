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
        return true;
    }

    return false;
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

// --------------------------------------------------------------------------------------- Fusion Downsample ---------------------------------------------------------------------------------------
FUSION_DOWNSAMPLE_THREAD_GROUP
void FusionDownsample(uint3 ID : SV_DispatchThreadID)
{
    UINT2 ReadPos = ID.xy * 2;
    UINT2 WritePos = ID.xy;
    
    BRANCH
    if (IsThreadOutWorkArea(WritePos, g_DestSize))
    {
        return;
    }
    
    float TotalWeight = 0.f;
    float3 TotalValue = 0.f;
    for (UINT i = 0;i < 2; ++i)
    {
        for (UINT j = 0; j < 2; ++j)
        {
            UINT2 SamplePos = ReadPos.xy + UINT2(i, j);
            if (SamplePos.x < g_SourceSize.x && SamplePos.y < g_SourceSize.y)
            {
                float3 SampleValue = LoadTexture2D(g_SourceTexIndex, SamplePos);
                TotalWeight += 1.f;
                TotalValue += SampleValue;
            }
        }
    }
    
    float3 Result = TotalValue * rcp(max(TotalWeight, 1e-5f));
    RWTexture2D<float3> OutputTex = ResourceDescriptorHeap[g_DestLumTexIndex];
    OutputTex[WritePos] = Result;
}

FUSION_BLEND_THREAD_GROUP
void FusionBlend(uint3 ID : SV_DispatchThreadID)
{
    UINT2 ReadPos = ID;
    
    if (IsThreadOutWorkArea(ReadPos, g_DestSize))
    {
        return;
    }
    
    float3 SourceLum = LoadTexture2D(g_LumTexIndex, ReadPos);
    float3 SourceWeight = LoadTexture2D(g_WeightTexIndex, ReadPos);
    
    float Result = 0.f;
    BRANCH
    if (!g_IsCoarsest)
    {
        float2 CoarseSampleUV = (float2(ReadPos) + 0.5f) * g_DestSize.zw;
        SourceLum -= SampleTexture2D(g_CoarseLumTexIndex, CoarseSampleUV, ClampLinearSampler);
        Result = SampleTexture2D(g_CoarseResultTexIndex, CoarseSampleUV, ClampLinearSampler);
    }
     
    RWTexture2D<float> ResultTex = ResourceDescriptorHeap[g_DestResultTexIndex];
    ResultTex[ReadPos] = Result + dot(SourceLum, SourceWeight);
}
