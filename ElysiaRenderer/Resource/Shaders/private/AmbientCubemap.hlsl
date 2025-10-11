#pragma once

#include "SharedCommon.hlsli"
#include "MonteCarlo.hlsl"
#include "BRDF.hlsl"
#include "Random.hlsl"
#include "LightAccumulator.hlsl"

#define IBLNumSamples 512

half ComputeCubemapMipFromRoughness(half Roughness, half MipCount)
{
	// Level starting from 1x1 mip
    half Level = 3 - 1.15 * log2(Roughness);
    return MipCount - 1 - Level;
}

float3 PrefilterEnvMap(uint2 Random, float Roughness, float3 R)
{
    float3 FilteredColor = 0;
    float Weight = 0;
    
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    TextureCube<float4> skyboxTex = ResourceDescriptorHeap[SkyboxTexIndex];
    const uint NumSamples = IBLNumSamples;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 E = Hammersley(i, NumSamples, Random);
        float3 H = TangentToWorld(ImportanceSampleGGX(E, Pow4(Roughness)).xyz, R);
        float3 L = 2 * dot(R, H) * H - R;

        float NoL = saturate(dot(R, L));
        if (NoL > 0)
        {
            FilteredColor += skyboxTex.SampleLevel(warpLinearSampler, L, 0).rgb * NoL;
            Weight += NoL;
        }
    }

    return FilteredColor / max(Weight, 0.001);
}

float3 DiffuseIBL(uint2 Random, float3 DiffuseColor, float Roughness, float3 N, float3 KD)
{
    N = normalize(N);
    
    float3 DiffuseLighting = 0;
	
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    TextureCube<float4> skyboxTex = ResourceDescriptorHeap[SkyboxTexIndex];
    DiffuseLighting += skyboxTex.SampleLevel(warpLinearSampler, N, 5) * KD;
    
    return DiffuseLighting;
}

float3 SpecularIBL(uint2 Random, float3 SpecularColor, float Roughness, float3 N, float3 V)
{
    float3 SpecularLighting = 0;
    
    float mipmapLevel = ComputeCubemapMipFromRoughness(Roughness, 5);
    
    float3 R = 2 * dot(V, N) * N - V;
    TextureCube<float4> skyboxTex = ResourceDescriptorHeap[SkyboxTexIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    float3 PrefilteredColor = skyboxTex.SampleLevel(warpLinearSampler, R, mipmapLevel);
    
    float NoV = saturate(dot(N, V));
    float2 EnvBRDF = EnvBRDFApprox(SpecularColor, Roughness, NoV).xy;
    
    SpecularLighting = PrefilteredColor * (SpecularColor * EnvBRDF.x + EnvBRDF.y);

    return SpecularLighting;
}

float3 GetIBL(FInputParams inputParams, MaterialData materialData, float3 toLight)
{
    float3 o = 0.f;
    
    float3 N = materialData.WorldNormal;
    float3 V = -inputParams.ScreenVector;
    float3 L = toLight;
    
    float3 R0 = 2 * dot(V, N) * N - V;
    float NoV = saturate(dot(N, V));
    float VoL = dot(V, L);
    float InvLenH = rsqrt(2 + 2 * VoL);
    float VoH = saturate(InvLenH + InvLenH * VoL);
    
    // Point lobe in off-specular peak direction
    float a = Square(materialData.Roughness);
    float3 R = lerp(N, R0, (1 - a) * (sqrt(1 - a) + a));
    uint2 Random = Rand3DPCG16(uint3(inputParams.PixelPos, frameIndex % 8)).xy;
    float3 KD = (1 - UE_F_Schlick(materialData.SpecularColor, VoH)) * (1 - materialData.Metallic);
    float3 NonSpecularContribution = 0;
    float3 SpecularContribution = 0;
    
    NonSpecularContribution += DiffuseIBL(Random, materialData.DiffuseColor, materialData.Roughness, N, KD);
    SpecularContribution += SpecularIBL(Random, materialData.SpecularColor, materialData.Roughness, N, V);

    FLightAccumulator LightAccumulator = (FLightAccumulator) 0;
    const bool bNeedsSeparateSubsurfaceLightAccumulation = false;
    
    // .rgb:AmbientCubemapTint*AmbientCubemapIntensity, a:unused
    half3 AmbientCubemapColor = ambientCubemapIntensity * ambientCubemapTint;
    
    LightAccumulator_Add(LightAccumulator, NonSpecularContribution + SpecularContribution, NonSpecularContribution, AmbientCubemapColor.rgb, bNeedsSeparateSubsurfaceLightAccumulation);
    o += GetLightAccumulator_Result(LightAccumulator);
    
    return o;
}