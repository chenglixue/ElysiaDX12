#ifndef SHARED_COMMON_H
#define SHARED_COMMON_H

#include "Macros.hlsli"
#include "Light.hlsli"
#include "Math.hlsli"
#include "Transform.hlsli"

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
    float4 CameraPosWS;
    float4x4 M_View;
    float4x4 M_Proj;
    float4 ScreenSize;
    
    Light lights[MAIN_LIGHT_NUM];
    uint   _FrameIndex;
    //float4 paddingPass[6];
    
}
cbuffer PerObjectBuffer : register(b0, perObjectSpace)
{
    float4x4 M_World;
    
    //float4 paddingObject[12];

}

Texture2D g_GGX_E_LUT : register(t0, perPassSpace);
Texture2D g_GGX_EAvg_LUT : register(t1, perPassSpace);
TextureCube g_SkyboxTex : register(t2, perPassSpace);

Texture2D g_albedoTexture : register(t0, perObjectSpace);
Texture2D g_normalTexture : register(t1, perObjectSpace);
Texture2D g_metallicTexture : register(t2, perObjectSpace);
Texture2D g_roughnessTexture : register(t3, perObjectSpace);

struct FInputParams
{
    float3 PositionWS;
    float3 PositionVS;
    float2 PixelPos;
    
    float2 objectUV;
    float2 ScreenUV;
    
    float3 TangentWS;
    float3 BitTangentWS;
    float3 NormalWS;
    float3 ScreenVector;
};

struct MaterialData
{
    // 0..1, white for SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE (apply BaseColor after scattering is more correct and less blurry)
    float3 BaseColor;
    float Opacity;
    
    // 0..1
    float Metallic;
    // 0..1
    float Specular;
    // 0..1
    float Roughness;
    // 0..1
    float AO;
    
    // normalized
    float3 WorldNormal;
    
    float3 DiffuseColor;
    // 0..1 (derived from BaseColor, Metalness, Specular)
    float3 SpecularColor;
    // 0..1, white for SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE (apply BaseColor after scattering is more correct and less blurry)
    
    float Depth;
    float4 Velocity;
    
    // 0..1 (derived from BaseColor, Metalness, Specular)
    float Anisotropy;
    
};

struct FDecodeGBufferData
{
    // normalized
    float3 WorldNormal;
    // normalized, only valid if HAS_ANISOTROPY_MASK in SelectiveOutputMask
    float3 WorldTangent;
    // 0..1 (derived from BaseColor, Metalness, Specular)
    float Anisotropy;
    float3 DiffuseColor;
    // 0..1 (derived from BaseColor, Metalness, Specular)
    float3 SpecularColor;
    // 0..1, white for SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE (apply BaseColor after scattering is more correct and less blurry)
    float3 BaseColor;
    // 0..1
    float Metallic;
    // 0..1
    float Specular;
    // 0..1
    float Roughness;
    // 0..1
    float AO;
    // 0..255 
    uint ShadingModelID;
    // 0..1
    float4 CustomData;
    float Depth;
    float4 Velocity;

    float3 SceneColor;
    float Opacity;
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