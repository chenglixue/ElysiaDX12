#include "private\Color.hlsl"

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
    UINT u_shoulder;
    UINT u_con;
    UINT u_soft;
    UINT u_con2;
    UINT u_clip;
    UINT u_scaleOnly;
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
    PSInput o = (PSInput) 0;
    
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
    PSOutput o = (PSOutput) 0;
    
    float4 color = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);

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
    
    o.target0 = float4(color.rgb, 1.f);
    
    return o;
}

float4 BlitPS(PSInput i) : SV_TARGET
{   
    half4 blitterValue = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);
    
    return blitterValue;
}
