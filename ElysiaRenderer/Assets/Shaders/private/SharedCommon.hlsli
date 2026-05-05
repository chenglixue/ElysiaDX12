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

#define Shading_Model_ID_Unlit 0
#define Shading_Model_ID_Default_Lit 1
#define Shading_Model_ID_Preintegrated_Skin 2
#define Shading_Model_ID_Subsurface_Profile 3
#define Shading_Model_ID_Hair 4
#define Shading_Model_ID_Eye 5
#define Shading_Model_ID_Cloth 6
#define Shading_Model_ID_Clear_Coat 7
#define Shading_Model_ID_Two_Sided_Foliage 8

#define Vector2  float2
#define Vector3  float3
#define Vector4  float4
#define Matrix   float4x4

#define UINT        uint
#define UINT2       uint2
#define UINT3       uint3
#define UINT4       uint4
#define int32_t     int
#define UINT64      uint64_t

cbuffer GlobalConstant : register(b0, perFrameSpace)
{
    Vector4 cameraPosWS;
    Light mainLight;

    Matrix shadowMatrix;
    Vector4 shadowSize;

    // Values used to linearize the Z buffer (http://www.humus.name/temp/Linearize%20depth.txt)
    // x = 1-far/near
    // y = far/near
    // z = x/far
    // w = y/far
    float4 g_ZBufferParams;

    UINT64 frameIndex;
    float nearZ;
    float farZ;
    UINT GGX_E_LUT_Index;

    UINT GGX_Eavg_LUT_Index;
    UINT SkyboxTexIndex;
    UINT ShadowTexIndex;
    UINT BlueNoiseTexIndex;

    UINT GBuffer0Index;
    UINT GBuffer1Index;
    UINT GBuffer2Index;
    UINT GBuffer3Index;

    UINT GBuffer4Index;
    UINT GBuffer5Index;
    UINT GBuffer6Index;
    UINT OpaqueDepthIndex;

    UINT OpaqueColorIndex;
    UINT g_AOIndex;
    bool g_EnableAO;
    bool g_EnableShadow;

    float g_MipBias;
    float g_ShadowRadius;
    UINT g_SobolBufferIndex;
    UINT g_ScramblingTileBufferIndex;

    UINT g_RankingTileBufferIndex;
    float g_RandomSeed;
};

struct DX12Vertex
{
    Vector3 position;
    Vector3 color;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
};

struct FInputParams
{
    float3 PositionWS;
    float3 PositionVS;
    float2 PixelPos;

    float2 objectUV;
    float2 ScreenUV;
    float2 ScreenSize;

    float3 TangentWS;
    float3 BitTangentWS;
    float3 NormalWS;
    float3 ScreenVector;

    float Linear01Depth;
    float LinearEyeDepth;
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

struct FEncodeGBufferData
{
    float3 WorldNormal;
    float3 WorldTangent;
    float Anisotropy;
    float3 DiffuseColor;
    float3 SpecularColor;
    float3 BaseColor;
    float Metallic;
    float Specular;
    float Roughness;
    float AO;
    uint ShadingModelID;
    float4 CustomData;
    float Depth;
    float2 Velocity;

    float3 SceneColor;
    float Opacity;
    float PerObjectData;
    float PerComputedShadow;
    float3 IBL;

    float3 SubsurfaceColor;
    float Curvature;

    float backLit;
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
    float2 Velocity;

    float3 SceneColor;
    float Opacity;
    float PerObjectData;
    float PerComputedShadow;
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