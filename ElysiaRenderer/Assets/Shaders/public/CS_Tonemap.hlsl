#include "private\Color.hlsl"
#include "private\BloomCommon.hlsli"

#pragma Compute Tonemap

#define Neutral 0
#define LMP 1
#define AMD 2
#define ACESFilm 3
#define Uncharted2 4
#define DX11DSK 5

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

[numthreads(8, 8, 1)]
void Tonemap(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    float2 screenUV = ((float2)dispatchThreadID.xy + 0.5f) * g_DestSize.zw;

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_DestTextureIndex];
    float4 finalColor = LoadTexture2D(g_DestTextureIndex, dispatchThreadID.xy);
    float4 bloomColor = SampleTexture2D(g_BloomTexIndex, screenUV, ClampLinearSampler);
    finalColor += bloomColor * g_BloomIntensity * rcp((float)BLOOM_MIPMAP_COUNT);
    finalColor *= g_LocalExposure;

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
        finalColor.rgb = DX11DSKTone(finalColor);
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