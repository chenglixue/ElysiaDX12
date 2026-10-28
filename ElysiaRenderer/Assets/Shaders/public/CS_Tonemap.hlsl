#include "private\Color.hlsl"
#include "private\BloomCommon.hlsli"

#define Neutral 0
#define LMP 1
#define AMD 2
#define ACESFilm 3
#define Uncharted2 4
#define DX11DSK 5
#define Filmic 6

#define DISPLAYMODE_SDR 0
#define DISPLAYMODE_FSHDR_Gamma22 1
#define DISPLAYMODE_FSHDR_SCRGB 2
#define DISPLAYMODE_HDR10_2084 3
#define DISPLAYMODE_HDR10DISPLAYMODE_HDR10_SCRGB_2084 4

cbuffer PassConstant : register(b0, perPassSpace)
{
    uint g_DestTextureIndex;
    float4 g_DestSize;
    UINT tonemapMode;
    UINT g_BloomTexIndex;
    float g_LocalExposure;
    UINT g_ExposureScaleBufferIndex;
    UINT g_PreExposureScaleBufferIndex;
    UINT g_AutoExposureEnable;
    UINT g_PreExposureEnable;
    UINT g_FusionEnable;
    UINT g_FusionTexIndex;
    float g_BloomIntensity;
    bool u_shoulder;
    bool u_con;
    bool u_soft;
    bool u_con2;
    bool u_clip;
    bool u_scaleOnly;
    UINT u_displayMode;
    matrix u_inputToOutputMatrix;
    uint4 u_ctl[24];
    float g_FilmSlope;
    float g_FilmToe;
    float g_FilmShoulder;
    float g_FilmBlackClip;
    float g_FilmWhiteClip;
}

#define A_GPU 1
#define A_HLSL 1
#include "private/ffx_a.h"

#define LPM_NO_SETUP 1

uint4 LpmFilterCtl(uint i)
{
    return u_ctl[i];
}

#include "private/ffx_lpm.h"

float GetOneOverPreExposureScale()
{
    RWStructuredBuffer<float2> PreExposureScaleBuffer = ResourceDescriptorHeap[g_PreExposureScaleBufferIndex];
    return rcp(max(PreExposureScaleBuffer[0].r, 1e-5f));
}

[numthreads(8, 8, 1)]
void Tonemap(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    float2 screenUV = ((float2)dispatchThreadID.xy + 0.5f) * g_DestSize.zw;

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_DestTextureIndex];
    StructuredBuffer<float4> exposureScaleBuffer = ResourceDescriptorHeap[g_ExposureScaleBufferIndex];
    float4 finalColor = LoadTexture2D(g_DestTextureIndex, dispatchThreadID.xy);
    float3 bloom = SampleTexture2D(g_BloomTexIndex, screenUV, ClampLinearSampler).rgb;
    bloom *= g_BloomIntensity * rcp((float)BLOOM_MIPMAP_COUNT);
    float autoExposureScale = 1.0f;
    float OneOverPreExposureScale = 1.f;
    if (g_AutoExposureEnable != 0)
        autoExposureScale = exposureScaleBuffer[0].r;
    if (g_PreExposureEnable != 0)
        OneOverPreExposureScale = GetOneOverPreExposureScale();
    // UE PostProcessTonemap.usf: local exposure scales scene color only.
    // Bloom is added after, with the same pre-exposure undo and global exposure.
    float exposureScale = g_LocalExposure * autoExposureScale * OneOverPreExposureScale;
    finalColor.rgb *= exposureScale;
    if (g_FusionEnable != 0)
    {
        float lum = max(dot(finalColor.rgb, float3(0.2126f, 0.7152f, 0.0722f)), 1e-5f);
        float fusion = SampleTexture2D(g_FusionTexIndex, screenUV, ClampLinearSampler).r;
        fusion = min(max(fusion, 0.0f), 0.999f);
        float fusedLum = (fusion * fusion) / max(1.0f - fusion * fusion, 1e-5f);
        finalColor.rgb *= fusedLum / lum;
    }
    finalColor.rgb += bloom * exposureScale;

    switch (tonemapMode)
    {
    case Neutral:
    {
        finalColor.rgb = NeutralTonemap(finalColor);
        break;
    }
    case LMP:
    {
        finalColor = mul(u_inputToOutputMatrix, finalColor);
        finalColor.r = max(0, finalColor.r);
        finalColor.g = max(0, finalColor.g);
        finalColor.b = max(0, finalColor.b);

        LpmFilter(finalColor.r, finalColor.g, finalColor.b, u_shoulder, u_con, u_soft, u_con2, u_clip, u_scaleOnly);
        break;
    }
    case AMD:
    {
        finalColor.rgb = AMDTonemapper(finalColor);
        break;
    }
    case ACESFilm:
    {
        finalColor.rgb = ACESFilmTone(finalColor);
        break;
    }
    case Uncharted2:
    {
        finalColor.rgb = Uncharted2Tonemap(finalColor);
        break;
    }
    case DX11DSK:
    {
        finalColor.rgb = DX11DSKTone(finalColor.rgb);
        break;
    }
    case Filmic:
    {
        // UE: tonemap in AP1 (ACEScg), then back to linear sRGB.
        float3 colorAP1 = mul(sRGB_2_AP1, finalColor.rgb);
        float3 toneAP1 = FilmToneMap(colorAP1,
                                     g_FilmSlope,
                                     g_FilmToe,
                                     g_FilmShoulder,
                                     g_FilmBlackClip,
                                     g_FilmWhiteClip);
        finalColor.rgb = max(mul(AP1_2_sRGB, toneAP1), 0.0.xxx);
        break;
    }
    }

    switch (u_displayMode)
    {
    case DISPLAYMODE_FSHDR_Gamma22:
    {
        finalColor.rgb = ApplyGamma(finalColor);
        break;
    }
    case DISPLAYMODE_HDR10_2084:
    {
        finalColor.rgb = ApplyPQ(finalColor);
        break;
    }
    }

    o[dispatchThreadID.xy] = float4(finalColor.rgb, 1.f);
}