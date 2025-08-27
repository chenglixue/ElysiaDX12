#pragma once
#include "private/SharedCommon.hlsli"

#include "private/Common.hlsl"
#include "private/BRDF.hlsl"
#include "private/Light.hlsl"
#include "private/LightCommon.hlsl"
#include "private/ShadingCommon.hlsl"

struct VSInput
{
    float3 positionOS   : POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normalOS : NORMAL;
    float3 tangentOS : TANGENT;
};

struct PSInput
{
    float4 positionCS   : SV_POSITION;
    float4 positionVS   : VIEW_POSITION;
    float4 positionWS   : WORLD_POSITION;
    float3 normalWS     : NORMAL;
    float3 tangentWS    : TANGENT;
    float3 bitTangentWS : BITTANGENT;
    float2 uv           : TEXCOORD;
    float3 color        : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o;

    o.positionWS = mul(M_World, float4(i.positionOS, 1.f));
    o.positionVS = mul(M_View, o.positionWS);
    o.positionCS = mul(M_Proj, o.positionVS);
    
    float3 N = normalize(mul(i.normalOS, (float3x3)M_World));
    float3 T = normalize(mul(i.tangentOS, (float3x3)M_World));
    
    o.tangentWS = normalize(T - dot(N, T) * N);
    o.bitTangentWS = cross(o.tangentWS, N);
    o.normalWS = N;
    
    o.uv = i.uv;
    o.color = i.color;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;
    
    FInputParams inputParam = (FInputParams) 0;
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.ScreenUV = i.uv;
    inputParam.ScreenVector = GetScreenVectorWS(CameraPosWS.xyz, i.positionWS.xyz);
    
    float3x3 TBN = float3x3(i.tangentWS, i.bitTangentWS, i.normalWS);
    
    float3 totalDirectRadiance = 0;
    float3 totalIndirectRadiance = 0;
    
    Light mainLight = GetMainLight(lights[0]);
    float3 toLight = -mainLight.direction;
    
    FDeferredLightingSplit lighting = (FDeferredLightingSplit) 0;
    FDecodeGBufferData GBufferData = GetDecodeGBufferData(inputParam.ScreenUV, TBN);
    
    o.target0.rgb = GBufferData.WorldNormal;
    return o;
}