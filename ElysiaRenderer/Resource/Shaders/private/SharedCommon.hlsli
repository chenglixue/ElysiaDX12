#ifndef SHARED_COMMON_H
#define SHARED_COMMON_H

#include "Macros.hlsli"
#include "Light.hlsli"
#include "Math.hlsli"

#define perObjectSpace   space0
#define perMaterialSpace space1
#define perPassSpace     space2
#define perFrameSpace    space3

SamplerState g_Sampler_WarpU_WarpV_Point : register(s0);
SamplerState g_Sampler_ClampU_ClampV_Point : register(s1);
SamplerState g_Sampler_WarpU_WarpV_Linear : register(s2);
SamplerState g_Sampler_ClampU_ClampV_Linear : register(s3);
SamplerState g_Sampler_WarpU_WarpV_Anisotropic : register(s4);
SamplerState g_Sampler_ClampU_ClampV_Anisotropic : register(s5);

cbuffer PerPassBuffer : register(b0, perPassSpace)
{
    float4      CameraPosWS;
    float4x4    M_View;
    float4x4    M_Proj;
    
    LightData lights[MAIN_LIGHT_NUM];
}
cbuffer PerObjectBuffer : register(b0, perObjectSpace)
{
    float4x4 M_World;
}

Texture2D g_albedoTexture : register(t0, perPassSpace);
Texture2D g_normalTexture : register(t1, perPassSpace);
Texture2D g_metallicTexture : register(t2, perPassSpace);
Texture2D g_roughnessTexture : register(t3, perPassSpace);

struct FInputParams
{
    float2 PixelPos;
    float4 ScreenPosition;
    float2 ScreenUV;
    float3 ScreenVector;
    float RawDepth;
    float Linear01Depth;
    float LinearEyeDepth;
};

struct BxDFContext
{
    float NoV;
    float NoL;
    float VoL;
    float NoH;
    float VoH;
    float XoV;
    float XoL;
    float XoH;
    float YoV;
    float YoL;
    float YoH;
};
#endif