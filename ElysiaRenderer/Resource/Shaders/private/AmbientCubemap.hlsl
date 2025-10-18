#ifndef AMBIENTCUBEMAP
#define AMBIENTCUBEMAP

#pragma once

#include "SharedCommon.hlsli"
#include "MonteCarlo.hlsl"
#include "BRDF.hlsl"
#include "Random.hlsl"
#include "LightAccumulator.hlsl"

#define IBLNumSamples 12

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
    DiffuseLighting += skyboxTex.SampleLevel(warpLinearSampler, N, 10) * KD;
    
    return DiffuseLighting;
}

float3 DiffuseIBLMul(uint2 Random, float3 DiffuseColor, float Roughness, float3 N, float3 V)
{
    N = normalize(N);
    V = normalize(V);

    float3 DiffuseLighting = 0;
	
    float NoV = saturate(dot(N, V));

    TextureCube<float4> skyboxTex = ResourceDescriptorHeap[SkyboxTexIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    const uint NumSamples = IBLNumSamples;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 E = Hammersley(i, NumSamples, Random);
        float3 L = TangentToWorld(CosineSampleHemisphere(E).xyz, N);
        float3 H = normalize(V + L);

        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        if (NoL > 0)
        {
            float3 SampleColor = skyboxTex.SampleLevel(warpLinearSampler, L, 0).rgb;

            float FD90 = (0.5 + 2 * VoH * VoH) * Roughness;
			//float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
            float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
            float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);

#if 1
			// lambert = DiffuseColor * NoL / PI
			// pdf = NoL / PI
            DiffuseLighting += SampleColor * DiffuseColor * FdV * FdL * (1 - 0.3333 * Roughness);
#else
			DiffuseLighting += SampleColor * DiffuseColor;
#endif
        }
    }

    return DiffuseLighting / NumSamples;
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

float3 IntegrateBRDF(uint2 Random, float Roughness, float NoV)
{
    float3 V;
    V.x = sqrt(1.0f - NoV * NoV); // sin
    V.y = 0;
    V.z = NoV; // cos

    float A = 0;
    float B = 0;
    float C = 0;

    const uint NumSamples = IBLNumSamples;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 E = Hammersley(i, NumSamples, Random);

		{
            float3 H = ImportanceSampleGGX(E, Pow4(Roughness)).xyz;
            float3 L = 2 * dot(V, H) * H - V;

            float NoL = saturate(L.z);
            float NoH = saturate(H.z);
            float VoH = saturate(dot(V, H));

            if (NoL > 0)
            {
                float a = Square(Roughness);
                float a2 = a * a;
                float Vis = Vis_SmithJointApprox(a2, NoV, NoL);
                float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
                float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
				//float Vis = 0.5 * rcp( Vis_SmithV + Vis_SmithL );

				// Incident light = NoL
				// pdf = D * NoH / (4 * VoH)
				// NoL * Vis / pdf
                float NoL_Vis_PDF = NoL * Vis * (4 * VoH / NoH);

                float Fc = pow(1 - VoH, 5);
                A += (1 - Fc) * NoL_Vis_PDF;
                B += Fc * NoL_Vis_PDF;
            }
        }

		{
            float3 L = CosineSampleHemisphere(E).xyz;
            float3 H = normalize(V + L);

            float NoL = saturate(L.z);
            float NoH = saturate(H.z);
            float VoH = saturate(dot(V, H));

            float FD90 = (0.5 + 2 * VoH * VoH) * Roughness;
            float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
            float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);
            C += FdV * FdL * (1 - 0.3333 * Roughness);
        }
    }

    return float3(A, B, C) / NumSamples;
}

float3 ApproximateSpecularIBL(uint2 Random, float3 SpecularColor, float Roughness, float3 N, float3 V)
{
	// Function replaced with prefiltered environment map sample
    float3 R = 2 * dot(V, N) * N - V;
    float3 PrefilteredColor = PrefilterEnvMap(Random, Roughness, R);
	//float3 PrefilteredColor = FilterEnvMap( Random, Roughness, N, V );

	// Function replaced with 2D texture sample
    float NoV = saturate(dot(N, V));
    float2 AB = IntegrateBRDF(Random, Roughness, NoV).xy;

    return PrefilteredColor * (SpecularColor * AB.x + AB.y);
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
    
    //NonSpecularContribution += DiffuseIBL(Random, materialData.DiffuseColor, materialData.Roughness, N, KD);
    NonSpecularContribution += DiffuseIBLMul(Random, materialData.DiffuseColor, materialData.Roughness, N, V);
    SpecularContribution += ApproximateSpecularIBL(Random, materialData.SpecularColor, materialData.Roughness, N, V);

    FLightAccumulator LightAccumulator = (FLightAccumulator) 0;
    const bool bNeedsSeparateSubsurfaceLightAccumulation = false;
    
    // .rgb:AmbientCubemapTint*AmbientCubemapIntensity, a:unused
    half3 AmbientCubemapColor = ambientCubemapIntensity * ambientCubemapTint;
    
    LightAccumulator_Add(LightAccumulator, NonSpecularContribution + SpecularContribution, NonSpecularContribution, AmbientCubemapColor.rgb, bNeedsSeparateSubsurfaceLightAccumulation);
    o += GetLightAccumulator_Result(LightAccumulator);
    
    return o;
}

#endif