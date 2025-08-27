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
    float4 normalWS     : NORMAL;
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
    
    o.normalWS = mul(M_World, float4(i.normalOS, 0.f));
    
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
    
    float4 baseColor = g_albedoTexture.Sample(g_Sampler_WarpU_WarpV_Linear, inputParam.ScreenUV);
    float4 normal = g_normalTexture.Sample(g_Sampler_WarpU_WarpV_Linear, inputParam.ScreenUV);
    float4 metallic = g_metallicTexture.Sample(g_Sampler_WarpU_WarpV_Linear, inputParam.ScreenUV);
    float4 roughness = g_roughnessTexture.Sample(g_Sampler_WarpU_WarpV_Linear, inputParam.ScreenUV);
    
    float3 totalDirectRadiance = 0;
    float3 totalIndirectRadiance = 0;
    
    Light mainLight = GetMainLight(lights[0]);
    
    
    FDeferredLightingSplit lighting = (FDeferredLightingSplit) 0;
    FDecodeGBufferData GBufferData = (FDecodeGBufferData) 0;
    
    o.target0.rgb = mainLight.color * normal;
    return o;
}