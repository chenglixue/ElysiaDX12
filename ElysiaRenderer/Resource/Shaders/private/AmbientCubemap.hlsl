#pragma once

#include "SharedCommon.hlsli"
#include "MonteCarlo.hlsl"
#include "BRDF.hlsl"
#include "Random.hlsl"
#include "LightAccumulator.hlsl"

#define IBLNumSamples 16

float3 DiffuseIBL(uint2 Random, float3 DiffuseColor, float Roughness, float3 N, float3 V)
{
    N = normalize(N);
    V = normalize(V);
    
    float3 DiffuseLighting = 0;
	
    float NoV = saturate(dot(N, V));
    
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
            float3 SampleColor = g_SkyboxTex.SampleLevel(g_Sampler_WarpU_WarpV_Linear, L, 0).rgb;

            float FD90 = (0.5 + 2 * VoH * VoH) * Roughness;
			//float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
            float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
            float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);

            // lambert = DiffuseColor * NoL / PI
			// pdf = NoL / PI
            DiffuseLighting += SampleColor * DiffuseColor * FdV * FdL * (1 - 0.3333 * Roughness);
        }
    }
    
    DiffuseLighting /= NumSamples;
    
    return DiffuseLighting;
}

float3 SpecularIBL(uint2 Random, float3 SpecularColor, float Roughness, float3 N, float3 V)
{
    float3 SpecularLighting = 0;

    const uint NumSamples = IBLNumSamples;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 E = Hammersley(i, NumSamples, Random);
        float3 H = TangentToWorld(ImportanceSampleGGX(E, Pow4(Roughness)).xyz, N);
        float3 L = 2 * dot(V, H) * H - V;

        float NoV = saturate(dot(N, V));
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
		
        if (NoL > 0)
        {
            float3 SampleColor = g_SkyboxTex.SampleLevel(g_Sampler_WarpU_WarpV_Linear, L, 0).rgb;

            float Vis = Vis_SmithJointApprox(Pow4(Roughness), NoV, NoL);
            float Fc = pow(1 - VoH, 5);
            float3 F = (1 - Fc) * SpecularColor + Fc;

			// Incident light = SampleColor * NoL
			// Microfacet specular = D*G*F / (4*NoL*NoV) = D*Vis*F
			// pdf = D * NoH / (4 * VoH)
            SpecularLighting += SampleColor * F * (NoL * Vis * (4 * VoH / NoH));
        }
    }

    return SpecularLighting / NumSamples;
}

float3 GetIBL(FInputParams inputParams, MaterialData materialData)
{
    float3 o = 0.f;
    
    float3 N = materialData.WorldNormal;
    float3 V = -inputParams.ScreenVector;
    
    float3 R0 = 2 * dot(V, N) * N - V;
    float NoV = saturate(dot(N, V));
    
    // Point lobe in off-specular peak direction
    float a = Square(materialData.Roughness);
    float3 R = lerp(N, R0, (1 - a) * (sqrt(1 - a) + a));
    uint2 Random = Rand3DPCG16(uint3(inputParams.PixelPos, _FrameIndex % 8)).xy;
    
    float3 NonSpecularContribution = 0;
    float3 SpecularContribution = 0;
    
    NonSpecularContribution += DiffuseIBL(Random, materialData.DiffuseColor, materialData.Roughness, N, V);
    SpecularContribution += SpecularIBL(Random, materialData.SpecularColor, materialData.Roughness, N, V);

    FLightAccumulator LightAccumulator = (FLightAccumulator) 0;
    const bool bNeedsSeparateSubsurfaceLightAccumulation = false;
    
    // .rgb:AmbientCubemapTint*AmbientCubemapIntensity, a:unused
    half3 AmbientCubemapColor = AmbientCubemapIntensity * AmbientCubemapTint;
    
    LightAccumulator_Add(LightAccumulator, NonSpecularContribution + SpecularContribution, NonSpecularContribution, AmbientCubemapColor.rgb, bNeedsSeparateSubsurfaceLightAccumulation);
    o += GetLightAccumulator_Result(LightAccumulator);
    
    return o;
}