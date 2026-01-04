#include "private\Color.hlsl"

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
    float4 color = LoadTexture2D(g_DestTextureIndex, dispatchThreadID.xy);

    switch (tonemapMode)
    {
    case Neutral:
    {
        color.rgb = NeutralTonemap(color);
        break;
    }
    case LMP:
    {
        color = mul(u_inputToOutputMatrix, color);
        color.r = max(0, color.r);
        color.g = max(0, color.g);
        color.b = max(0, color.b);

        LpmFilter(color.r, color.g, color.b, u_shoulder, u_con, u_soft, u_con2, u_clip, u_scaleOnly);
        break;
    }
    case AMD:
    {
        color.rgb = AMDTonemapper(color);
        break;
    }
    case ACESFilm:
    {
        color.rgb = ACESFilmTone(color);
        break;
    }
    case Uncharted2:
    {
        color.rgb = Uncharted2Tonemap(color);
        break;
    }
    case DX11DSK:
    {
        color.rgb = DX11DSKTone(color);
        break;
    }
    }

    switch (u_displayMode)
    {
    case DISPLAYMODE_FSHDR_Gamma22:
    {
        color.rgb = ApplyGamma(color);
        break;
    }
    case DISPLAYMODE_HDR10_2084:
    {
        color.rgb = ApplyPQ(color);
        break;
    }
    }

    o[dispatchThreadID.xy] = float4(color.rgb, 1.f);
}