#ifndef SHADING_COMMON_H
#define SHADING_COMMON_H

#pragma once

#include "SharedCommon.hlsli"
#include "AmbientCubemap.hlsl"

///////////////////////////////////////////////////////////////////////////////
// Shading parameterisation

float F0ToDielectricSpecular(float F0)
{
    return saturate(F0 / 0.08f);
}

float F0RGBToF0(float3 F0)
{
    return dot(0.3333333.xxx, F0);
}

float F0RGBToDielectricSpecular(float3 F0)
{
    return F0ToDielectricSpecular(F0RGBToF0(F0));
}

float DielectricSpecularToF0(float Specular)
{
    return float(0.08f * Specular);
}

// [Burley, "Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering"]
float DielectricF0ToIor(float F0)
{
    return 2.0f / (1.0f - sqrt(min(F0, 0.99))) - 1.0f;
}

float DielectricF0RGBToIor(float3 F0)
{
    return DielectricF0ToIor(F0RGBToF0(F0));
}

float DielectricIorToF0(float Ior)
{
    const float F0Sqrt = (Ior - 1) / (Ior + 1);
    const float F0 = F0Sqrt * F0Sqrt;
    return F0;
}

// Anything with Specular less than 2% is physically impossible and is instead considered to be shadowing.
float GetF0MicroOcclusionThreshold()
{
    return 0.02f;
}
float F0ToMicroOcclusion(float F0)
{
    return saturate(50.0 * F0);
}
float3 F0ToMicroOcclusion(float3 F0)
{
    return saturate(50.0 * F0);
}

float F0RGBToMicroOcclusion(float3 F0)
{
    return F0ToMicroOcclusion(max(F0.r, max(F0.g, F0.b)));
}

float3 ComputeF0(float Specular, float3 BaseColor, float Metallic)
{
    return lerp(DielectricSpecularToF0(Specular).xxx, BaseColor, Metallic.xxx);
}

float3 ComputeF90(float3 F0, float3 EdgeColor, float Metallic)
{
    return lerp(1.0, EdgeColor, Metallic.xxx);
}

float3 ComputeDiffuseAlbedo(float3 BaseColor, float Metallic)
{
    return BaseColor - BaseColor * Metallic;
}

float MakeRoughnessSafe(float Roughness, float MinRoughness = 0.001f)
{
    return clamp(Roughness, MinRoughness, 1.0f);
}

float F0ToMetallic(float F0)
{
	// Approximate the metallic input from F0 with a small lerp region
    const float FullMetalBeginF0 = 0.08f; // Instead of DiamondF0 = 0.24, the metallic region starts right after metallic >0 and specular=1 to match with legacy.
    const float FullMetalEndF0 = 0.4f; // roughly the end of semi-conductor
	// This is compatible with UE shading model mapping allowing F0 to take a value up to 0.08 for dielectric.

    return saturate((F0 - FullMetalBeginF0) / (FullMetalEndF0 - FullMetalBeginF0));
}

float F0RGBToMetallic(float3 F0)
{
    return F0ToMetallic(max(F0.r, max(F0.g, F0.b)));
}

float3 EncodeNormal(float3 N)
{
    return N * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 N)
{
    return N * 2.f - 1.f;
    //return OctahedronToUnitVector( Pack888To1212( N ) * 2 - 1 );
}

float EncodeMaterialFlags(uint materialFlags)
{
    return materialFlags * (1.0h / 255.0h);
}

uint DecodeMaterialFlags(float packedMaterialFlags)
{
    return uint((packedMaterialFlags * 255.0h) + 0.5h);
}

SamplerData GetSamplerData()
{
    SamplerData o = (SamplerData) 0;
    
    o.clampPointSampler = SamplerDescriptorHeap[ClampPointSampler];
    o.clampLinearSampler = SamplerDescriptorHeap[ClampLinearSampler];
    o.clampAnisotropicSampler = SamplerDescriptorHeap[ClampAnisotropicSampler];
    o.warpPointSampler = SamplerDescriptorHeap[WarpPointSampler];
    o.warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    o.warpAnisotropicSampler = SamplerDescriptorHeap[WarpAnisotropicSampler];
    o.shadowClampLinearSampler = SamplerDescriptorHeap[ShadowClampLinearSampler];
    o.shadowWarpLinearSampler = SamplerDescriptorHeap[ShadowWarpLinearSampler];
    
    return o;
}

MaterialData GetMaterialData(FInputParams inputParams)
{
    MaterialData o = (MaterialData) 0;
    
    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
    float4 baseColor = baseColorTex.Sample(warpLinearSampler, inputParams.objectUV)
            * float4(baseColorTint, opacity);
    clip(baseColor.a - cutoff);

    Texture2D<float4>  normalTex = ResourceDescriptorHeap[normalTexIndex];
    float4 normalTS = normalTex.Sample(warpLinearSampler, inputParams.objectUV);

    Texture2D<float>  metallicTex = ResourceDescriptorHeap[metallicTexIndex];
    float metallic = metallicTex.Sample(warpLinearSampler, inputParams.objectUV);
    metallic = metallic * metallicIntensity;
    
    Texture2D<float>  roughnessTex = ResourceDescriptorHeap[roughnessTexIndex];
    float roughness = roughnessTex.Sample(warpLinearSampler, inputParams.objectUV);
    roughness = roughness * roughnessIntensity;

    o.BaseColor = baseColor.rgb;
    o.Opacity = baseColor.a;
    o.AO = 1;
    o.Metallic = metallic;
    o.Roughness = roughness;
    o.Specular = 0.5;
    
    //o.WorldNormal = g_hasNormalTex ? GetNormal(normalTS.rgb, TBN, normalIntensity) : inputParams.NormalWS;
    o.WorldNormal = GetNormal(normalTS.rgb, TBN, normalIntensity, true);

    o.Anisotropy = 0;
    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);

    return o;
}

FEncodeGBufferData GetEncodeGBufferData(FInputParams inputParams, float3 toLight)
{
    FEncodeGBufferData o = (FEncodeGBufferData) 0;
    
    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
    float4 baseColor = baseColorTex.Sample(warpLinearSampler, inputParams.objectUV)
            * float4(baseColorTint, opacity);
    clip(baseColor.a - cutoff);

    Texture2D<float4> normalTex = ResourceDescriptorHeap[normalTexIndex];
    float4 normalTS = normalTex.Sample(warpLinearSampler, inputParams.objectUV);

    Texture2D<float> metallicTex = ResourceDescriptorHeap[metallicTexIndex];
    float metallic = metallicTex.Sample(warpLinearSampler, inputParams.objectUV);
    metallic = metallic * metallicIntensity;
    
    Texture2D<float> roughnessTex = ResourceDescriptorHeap[roughnessTexIndex];
    float roughness = roughnessTex.Sample(warpLinearSampler, inputParams.objectUV);
    roughness = roughness * roughnessIntensity;
    
    o.BaseColor = baseColor.rgb;
    o.ShadingModelID = FLT_MAX;
    o.ShadingModelID = Shading_Model_ID_Default_Lit;
    o.Opacity = baseColor.a;
    
    o.AO = 1;
    o.Metallic = metallic;
    o.Roughness = roughness;
    o.Specular = 0.5;
    
    o.WorldNormal = GetNormal(normalTS.rgb, TBN, normalIntensity);
    o.WorldTangent = TBN[0];
    o.PerObjectData = 0.f;
    o.PerComputedShadow = 1.f;
    
    o.Velocity = 0.f;
    
    o.Anisotropy = 0;
    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);
    o.IBL = GetIBL(inputParams, o, toLight);

    return o;
}

FDecodeGBufferData DecodeGBufferData(float4 InGBuffer0,
    float4 InGBuffer1,
    float4 InGBuffer2,
    float4 InGBuffer3,
    float4 InGBuffer4,
    float4 InGBufferVelocity,
    float SceneDepth)
{
    FDecodeGBufferData o = (FDecodeGBufferData) 0;

    o.BaseColor = InGBuffer0.rgb;
    o.ShadingModelID = DecodeMaterialFlags(InGBuffer0.a);

    o.Metallic = InGBuffer1.r;
    o.Specular = InGBuffer1.g;
    o.Roughness = InGBuffer1.b;
    o.AO = InGBuffer1.a;

    o.WorldTangent = DecodeNormal(InGBuffer2.rgb);
    o.Anisotropy = InGBuffer2.a;

    o.WorldNormal = DecodeNormal(InGBuffer3.rgb);

    o.SceneColor = InGBuffer4.rgb;
    o.Opacity = InGBuffer4.a;

    o.Velocity = InGBufferVelocity.rg;

    o.CustomData = 0;

    o.Depth = SceneDepth;

    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);
    
    return o;
}

FDecodeGBufferData GetDecodeGBufferData(float2 uv)
{
    FDecodeGBufferData o = (FDecodeGBufferData) 0;
    
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D _GBuffer0 = ResourceDescriptorHeap[GBuffer0Index];
    Texture2D _GBuffer1 = ResourceDescriptorHeap[GBuffer1Index];
    Texture2D _GBuffer2 = ResourceDescriptorHeap[GBuffer2Index];
    Texture2D _GBuffer3 = ResourceDescriptorHeap[GBuffer3Index];
    Texture2D _GBuffer4 = ResourceDescriptorHeap[GBuffer4Index];
    Texture2D _GBuffer5 = ResourceDescriptorHeap[GBuffer5Index];
    Texture2D _OpaqueDepthRT = ResourceDescriptorHeap[OpaqueDepthIndex];
    
    float4 GBuffer0 = _GBuffer0.SampleLevel(warpLinearSampler, uv, 0);
    float4 GBuffer1 = _GBuffer1.SampleLevel(warpLinearSampler, uv, 0);
    float4 GBuffer2 = _GBuffer2.SampleLevel(warpLinearSampler, uv, 0);
    float4 GBuffer3 = _GBuffer3.SampleLevel(warpLinearSampler, uv, 0);
    float4 GBuffer4 = _GBuffer4.SampleLevel(warpLinearSampler, uv, 0);
    float4 GBuffer5 = _GBuffer5.SampleLevel(warpLinearSampler, uv, 0);
    float sceneDepth = _OpaqueDepthRT.SampleLevel(warpLinearSampler, uv, 0);

    o = DecodeGBufferData(GBuffer0, GBuffer1, GBuffer2, GBuffer3,
        GBuffer4, GBuffer5, sceneDepth);

    return o;
}

// Computes world-space position from post-projection depth
float3 PositionFromDepth(in float zw, in float2 uv)
{
    float linearDepth = projMatrix._43 / (zw - projMatrix._33);
    float4 positionCS = float4(uv * 2.0f - 1.0f, zw, 1.0f);
    positionCS.y *= -1.0f;
    float4 positionWS = mul(positionCS, viewProjMatrix_I);
    return positionWS.xyz / positionWS.w;
}

float4 ComputeClipSpacePosition(float2 screenUV, float rawDepth)
{
    float4 positionCS = float4(screenUV * 2.0 - 1.0, rawDepth, 1.0);

    positionCS.y = -positionCS.y;

    return positionCS;
}

float3 ComputeWorldSpacePosition(float2 screenUV, float rawDepth, Matrix invViewProjMatrix)
{
    float4 positionCS = ComputeClipSpacePosition(screenUV, rawDepth);
    float4 positionWS = mul(positionCS, invViewProjMatrix);
    return positionWS.xyz / positionWS.w;
}
#endif