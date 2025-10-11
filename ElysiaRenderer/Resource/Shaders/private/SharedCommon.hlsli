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

#define WarpPointSampler 0
#define ClampPointSampler 1
#define WarpLinearSampler 2
#define ClampLinearSampler 3
#define WarpAnisotropicSampler 4
#define ClampAnisotropicSampler 5
#define ShadowWarpLinearSampler 6
#define ShadowClampLinearSampler 7

#define Vector2  float2
#define Vector3  float3
#define Vector4  float4
#define Matrix   float4x4

#define UINT        uint
#define int32_t     int

struct DX12Vertex
{
    Vector3 position;
    Vector3 color;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
};

cbuffer ObjectConstant : register(b0, perObjectSpace)
{
    Matrix worldMatrix;

	Vector3 baseColorTint;
    float opacity;

    float normalIntensity;
    float metallicIntensity;
    float roughnessIntensity;
    float ambientCubemapIntensity;

	Vector3 ambientCubemapTint;
    int baseColorTexIndex;

	int normalTexIndex;
    int metallicTexIndex;
    int roughnessTexIndex;
    int specularTexIndex;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 cameraPosWS;
    Matrix  viewMatrix;
    Matrix  projMatrix;
    Matrix  shadowMatrix;
    Vector4 screenSize;
	Vector4 shadowSize;
    

    Light mainLight;
     
    UINT frameIndex;
    float nearZ;
    float farZ;
    float shadowNearZ;
    float shadowFarZ;
    int GGX_E_LUT_Index;
    
    int GGX_Eavg_LUT_Index;
    int SkyboxTexIndex;
    int ShadowTexIndex;
};

//cbuffer PerShadowPassBuffer : register(b1, perPassSpace)
//{
//    float4x4 M_ShadowView;
//    float4x4 M_ShadowProj;
    
//    float4x4 M_Shadow;
    
//    float ShadowNearZ;
//    float ShadowFarZ;
//}

//Texture2D g_GGX_E_LUT : register(t0, perPassSpace);
//Texture2D g_GGX_EAvg_LUT : register(t1, perPassSpace);
//TextureCube g_SkyboxTex : register(t2, perPassSpace);
//Texture2D g_ShadowTex : register(t3, perPassSpace);

//Texture2D g_albedoTexture : register(t0, perObjectSpace);
//Texture2D g_normalTexture : register(t1, perObjectSpace);
//Texture2D g_metallicTexture : register(t2, perObjectSpace);
//Texture2D g_roughnessTexture : register(t3, perObjectSpace);

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