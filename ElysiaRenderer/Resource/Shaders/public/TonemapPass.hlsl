#define A_GPU 1
#define A_HLSL 1

#define LPM_NO_SETUP 1

uint4 LpmFilterCtl(uint i);

#if EDITOR
#include <private\Color.hlsl>
#include <private\ffx_a.h> 
#include <private\ffx_lpm.h>
#else
#include "../private\Color.hlsl"
#include "../../../AMD\LPM\ffx_a.h"
#include "../../../AMD\LPM\ffx_lpm.h"
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT blitterTextureIndex;
    
    bool u_shoulder;
    bool u_con;
    bool u_soft;
    bool u_con2;
    bool u_clip;
    bool u_scaleOnly;
    uint u_displayMode;
    matrix u_inputToOutputMatrix;
    uint4 u_ctl[24];
}

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
    color = mul(color, u_inputToOutputMatrix);
    color.r = max(0, color.r);
    color.g = max(0, color.g);
    color.b = max(0, color.b);
    
    //float3 tonemapColor = NeutralTonemap(color);
    //tonemapColor = ToneMapFilmicALU(color);
    //tonemapColor = uncharted2_filmic(color);
    LpmFilter(color.r, color.g, color.b, u_shoulder, u_con, u_soft, u_con2, u_clip, u_scaleOnly);
    
    o.target0 = float4(color.rgb, 1.f);
    
    return o;
}

float4 BlitPS(PSInput i) : SV_TARGET
{   
    half4 blitterValue = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);
    
    return blitterValue;
}

uint4 LpmFilterCtl(uint i)
{
    return u_ctl[i];
}