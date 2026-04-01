#include "private\Color.hlsl"
#include "private/BloomCommon.hlsli"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Disabled

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
    uint blitterTextureIndex;
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

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput)0;

    if (vertexID == 0)
    {
        o.positionCS = float4(-1.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 0.0f);
    }
    else if (vertexID == 1)
    {
        o.positionCS = float4(3.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(2.0f, 0.0f);
    }
    else
    {
        o.positionCS = float4(-1.0f, -3.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 2.0f);
    }

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    float4 color = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);
    float4 bloomColor = float4(SampleTexture2D(g_BloomTexIndex, i.uv, ClampLinearSampler).rgb, 1.f) * g_BloomIntensity *
                        rcp((float)BLOOM_MIPMAP_COUNT);
    float4 finalColor = bloomColor + color;
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
    finalColor.rgb = LinearToSRGB(finalColor.rgb);
    o.target0 = float4(finalColor.rgb, 1.f);

    return o;
}