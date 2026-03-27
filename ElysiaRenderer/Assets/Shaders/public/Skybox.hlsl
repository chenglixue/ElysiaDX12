#include "private\ShadingCommon.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Enabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewProjMatrix;
    Matrix pre_viewProjMatrix;
    Matrix jitterViewProjMatrix;
}

struct VSInput
{
    float3 positionOS : POSITION;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float3 uv : TEXCOORD;
    float4 currNonJitterPosCS : TEXCOOR1;
    float4 preNonJitterPosCS : TEXCOOR2;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
    float2 Veloctiy : SV_TARGET1;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput)0;

    o.positionCS = mul(float4(i.positionOS, 1.f), jitterViewProjMatrix).xyww;;

    o.uv = i.positionOS.xyz;
    o.currNonJitterPosCS = mul(float4(i.positionOS, 1.f), viewProjMatrix).xyww;
    o.preNonJitterPosCS = mul(float4(i.positionOS, 1.f), pre_viewProjMatrix).xyww;

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    float2 preNDCPos = i.preNonJitterPosCS.xy / i.preNonJitterPosCS.w;
    float2 preScreenUV = preNDCPos.xy * 0.5f * float2(1.f, -1.f) + 0.5f;

    float2 currNDCPos = i.currNonJitterPosCS.xy / i.currNonJitterPosCS.w;
    float2 currScreenUV = currNDCPos.xy * 0.5f * float2(1.f, -1.f) + 0.5f;

    o.target0 = SampleTextureCube(SkyboxTexIndex, i.uv.xyz, WarpLinearSampler);
    o.Veloctiy = currScreenUV - preScreenUV;
    return o;
}