#pragma once

#include "SharedCommon.hlsli"

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

FDecodeGBufferData GetDecodeGBufferData(float2 uv, bool bGetNormalizedNormal = true)
{
    FDecodeGBufferData o = (FDecodeGBufferData) 0;
    
    
    

    return o;
}