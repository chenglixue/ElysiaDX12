#include "private\ShadingCommon.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Enabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
}

struct VSInput
{
    float3 positionOS : POSITION;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float3 uv : TEXCOORD;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput)0;

    o.positionCS = mul(float4(i.positionOS, 1.f), viewProjMatrix).xyww;;

    o.uv = i.positionOS.xyz;

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    o.target0 = SampleTextureCube(SkyboxTexIndex, i.uv.xyz, WarpLinearSampler);
    return o;
}