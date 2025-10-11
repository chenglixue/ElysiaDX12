#pragma once

#include "SharedCommon.hlsli"

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

MaterialData GetMaterialData(FInputParams inputParams)
{
    MaterialData o = (MaterialData) 0;
    
    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<float4> baseColorTex;
    float4 baseColor = float4(baseColorTint, opacity);
    Texture2D<float4> normalTex;
    float4 normalTS;
    Texture2D<float> metallicTex;
    Texture2D<float> roughnessTex;
    float metallic = metallicIntensity;
    float roughness = roughnessIntensity;
    if (baseColorTexIndex != -1)
    {
        baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
        baseColor = baseColorTex.Sample(warpLinearSampler, inputParams.objectUV)
            * float4(baseColorTint, opacity);
        clip(baseColor.a - 0.5);

    }

    if (normalTexIndex != -1)
    {
        normalTex = ResourceDescriptorHeap[normalTexIndex];
        normalTS = normalTex.Sample(warpLinearSampler, inputParams.objectUV);
    }

    if (metallicTexIndex != -1)
    {
        metallicTex = ResourceDescriptorHeap[metallicTexIndex];
        metallic = metallicTex.Sample(warpLinearSampler, inputParams.objectUV);
        metallic = metallic * metallicIntensity;
    }
    
    if (roughnessTexIndex != -1)
    {
        roughnessTex = ResourceDescriptorHeap[roughnessTexIndex];
        roughness = roughnessTex.Sample(warpLinearSampler, inputParams.objectUV);
        roughness = roughness * roughnessIntensity;
    }

    o.BaseColor = baseColor.rgb;
    o.Opacity = baseColor.a;
    o.AO = 1;
    o.Metallic = metallic;
    o.Roughness = roughness;
    o.Specular = 0.5;
    
    o.WorldNormal = GetNormal(normalTS.rgb, TBN, normalIntensity);

    o.Anisotropy = 0;
    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);

    return o;
}

FDecodeGBufferData GetDecodeGBufferData(float2 uv, float3x3 TBN, bool bGetNormalizedNormal = true)
{
    FDecodeGBufferData o = (FDecodeGBufferData) 0;
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<half4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
    Texture2D<half4> normalTex = ResourceDescriptorHeap[normalTexIndex];
    
    float4 temp = baseColorTex.Sample(warpLinearSampler, uv);
    
    o.BaseColor = temp.rgb;
    o.Opacity = temp.a;
    
    float3 normalTS = normalTex.Sample(warpLinearSampler, uv);

    return o;
}