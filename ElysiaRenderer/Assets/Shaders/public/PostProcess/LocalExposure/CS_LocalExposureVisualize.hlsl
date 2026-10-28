#include "private\Common.hlsl"

// UE PostProcessVisualizeLocalExposure.usf, Fusion permutation.
// r.LocalExposure.VisualizationMode. Bilateral modes 3, 4 and 5 are not produced.
#define VIS_OVERVIEW 0
#define VIS_LOCAL_EXPOSURE 1
#define VIS_THRESHOLDS 2
#define VIS_FUSION_BASE 6
#define VIS_FUSION_SHADOWS 7
#define VIS_FUSION_HIGHLIGHTS 8
#define VIS_FUSION_WEIGHTS 9

#define VIS_THREAD_GROUP [numthreads(GROUP_SIZE, GROUP_SIZE, 1)]

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_DestSize;
    UINT g_DestTexIndex;
    UINT g_HdrTexIndex;
    UINT g_SceneTexIndex;
    UINT g_LumTexIndex;
    UINT g_WeightTexIndex;
    UINT g_FusionTexIndex;
    UINT g_ExposureScaleBufferIndex;
    UINT g_PreExposureScaleBufferIndex;
    UINT g_Mode;
    UINT g_AutoExposureEnable;
    UINT g_PreExposureEnable;
    UINT g_HasFusion;
    float g_MiddleGrey;
    float g_HighlightThreshold;
    float g_ShadowThreshold;
}

float Rec709Luminance(float3 color)
{
    return max(dot(color, float3(0.2126f, 0.7152f, 0.0722f)), 1e-6f);
}

float OneOverPreExposure()
{
    if (g_PreExposureEnable == 0)
        return 1.0f;
    RWStructuredBuffer<float2> preExposure = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    return rcp(max(preExposure[0].r, 1e-5f));
}

// Same curve as FusionTonemap / the tonemap fusion apply: sqrt(L / (1 + L)).
float FusionInverseTonemap(float tonemapped)
{
    tonemapped = min(max(tonemapped, 0.0f), 0.999f);
    float squared = tonemapped * tonemapped;
    return squared / max(1.0f - squared, 1e-5f);
}

float DiagonalStripes(float2 screenPos, float thickness)
{
    float v = frac((screenPos.x + screenPos.y) / thickness);
    return step(0.5f, v);
}

// UE tile index is column + row * 4. Empty cells keep the tonemapped scene.
int OverviewMode(uint2 cell)
{
    if (cell.y == 0 && cell.x == 0)
        return VIS_LOCAL_EXPOSURE;
    if (cell.y == 0 && cell.x == 3)
        return VIS_THRESHOLDS;
    if (cell.y == 3 && cell.x == 0)
        return VIS_FUSION_SHADOWS;
    if (cell.y == 3 && cell.x == 1)
        return VIS_FUSION_BASE;
    if (cell.y == 3 && cell.x == 2)
        return VIS_FUSION_HIGHLIGHTS;
    if (cell.y == 3 && cell.x == 3)
        return VIS_FUSION_WEIGHTS;
    return -1;
}

float3 VisualizeMode(int mode, float2 uv, float2 patternPos, float3 sceneColor)
{
    float exposureScale = 1.0f;
    float middleGreyLum = max(g_MiddleGrey, 1e-6f);
    if (g_AutoExposureEnable != 0)
    {
        StructuredBuffer<float4> exposure = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
        exposureScale = max(exposure[0].r, 1e-6f);
        middleGreyLum = max(exposure[0].a, 1e-6f);
    }

    float3 hdr = SampleTexture2D_LOD(g_HdrTexIndex, uv, ClampLinearSampler, 0).rgb * OneOverPreExposure();
    float rawLum = Rec709Luminance(hdr);
    float logLum = log2(rawLum);
    float baseLogLum = logLum + log2(exposureScale);
    float baseCentered = baseLogLum - log2(middleGreyLum);

    // Setup stores (base, shadow, highlight). UE stores (base, highlight, shadow).
    // Views below use UE's names, so Highlights reads our .b and Shadows reads our .g.
    float3 exposures = 0.0f;
    float3 weights = 0.0f;
    float fusionVal = 0.0f;
    float localExposure = 1.0f;
    if (g_HasFusion != 0)
    {
        exposures = SampleTexture2D_LOD(g_LumTexIndex, uv, ClampLinearSampler, 0).rgb;
        weights = SampleTexture2D_LOD(g_WeightTexIndex, uv, ClampLinearSampler, 0).rgb;
        fusionVal = SampleTexture2D_LOD(g_FusionTexIndex, uv, ClampLinearSampler, 0).r;
        float exposedLum = Rec709Luminance(hdr * exposureScale);
        localExposure = FusionInverseTonemap(fusionVal) / exposedLum;
    }

    if (mode == VIS_LOCAL_EXPOSURE)
    {
        float amount = saturate(abs(log2(max(localExposure, 1e-6f)) / 2.0f));
        float3 tint = lerp(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), step(localExposure, 1.0f));
        return amount * tint;
    }

    if (mode == VIS_THRESHOLDS)
    {
        float3 outValue = sceneColor;
        if (baseCentered > 0.0f)
        {
            if (baseCentered - g_HighlightThreshold > 0.0f)
                outValue = lerp(outValue, float3(0.0f, 1.0f, 0.0f), DiagonalStripes(patternPos, 32.0f));
        }
        else if (baseCentered + g_ShadowThreshold < 0.0f)
        {
            outValue = lerp(outValue, float3(1.0f, 0.0f, 0.0f), DiagonalStripes(patternPos, 32.0f));
        }
        return outValue;
    }

    if (mode == VIS_FUSION_BASE)
        return exposures.r;
    if (mode == VIS_FUSION_HIGHLIGHTS)
        return exposures.b;
    if (mode == VIS_FUSION_SHADOWS)
        return exposures.g;
    if (mode == VIS_FUSION_WEIGHTS)
    {
        // UE swaps r/b on its (base, highlight, shadow) weights.
        // red = shadows, green = highlights, blue = base.
        return float3(weights.g, weights.b, weights.r);
    }

    return sceneColor;
}

VIS_THREAD_GROUP
void VisualizeLocalExposure(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel = id.xy;
    uint width = (uint)g_DestSize.x;
    uint height = (uint)g_DestSize.y;
    if (pixel.x >= width || pixel.y >= height)
        return;

    float2 uv = ((float2)pixel + 0.5f) * g_DestSize.zw;
    float3 sceneColor = SampleTexture2D_LOD(g_SceneTexIndex, uv, ClampLinearSampler, 0).rgb;
    int mode = (int)g_Mode;
    float2 patternPos = (float2)pixel;
    bool tileBorder = false;

    if (g_Mode == VIS_OVERVIEW)
    {
        uint tileW = width / 4;
        uint tileH = height / 4;
        if (tileW == 0 || tileH == 0 || pixel.x >= tileW * 4 || pixel.y >= tileH * 4)
        {
            RWTexture2D<float4> dest = ResourceDescriptorHeap[g_DestTexIndex];
            dest[pixel] = float4(sceneColor, 1.0f);
            return;
        }

        uint2 cell = uint2(pixel.x / tileW, pixel.y / tileH);
        uint2 local = uint2(pixel.x - cell.x * tileW, pixel.y - cell.y * tileH);
        mode = OverviewMode(cell);
        if (mode < 0)
        {
            RWTexture2D<float4> dest = ResourceDescriptorHeap[g_DestTexIndex];
            dest[pixel] = float4(sceneColor, 1.0f);
            return;
        }

        uv = ((float2)local + 0.5f) / float2(tileW, tileH);
        sceneColor = SampleTexture2D_LOD(g_SceneTexIndex, uv, ClampLinearSampler, 0).rgb;
        // The tile is a 1/4 render stretched back up, so stripe space is the tile pixel.
        patternPos = (float2)local;
        tileBorder = local.x == 0 || local.y == 0 || local.x + 1 == tileW || local.y + 1 == tileH;
    }

    float3 color = tileBorder ? 0.0f : VisualizeMode(mode, uv, patternPos, sceneColor);
    RWTexture2D<float4> dest = ResourceDescriptorHeap[g_DestTexIndex];
    dest[pixel] = float4(color, 1.0f);
}
