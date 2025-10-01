#pragma once

#include "SharedCommon.hlsli"
#include "BRDF.hlsl"
#include "AreaLightCommon.hlsl"
#include "LightAccumulator.hlsl"
#include "EnergyPreservation.hlsl"

struct FShadowTerms
{
    half SurfaceShadow;
    half TransmissionShadow;
    half TransmissionThickness;
};

struct FDirectLighting
{
    float3 Diffuse;
    float3 Specular;
};

float New_a2(float a2, float SinAlpha, float VoH)
{
    return a2 + 0.25 * SinAlpha * (3.0 * sqrt(a2) + SinAlpha) / (VoH + 0.001);
    //return a2 + 0.25 * SinAlpha * ( saturate(12 * a2 + 0.125) + SinAlpha ) / ( VoH + 0.001 );
    //return a2 + 0.25 * SinAlpha * ( a2 * 2 + 1 + SinAlpha ) / ( VoH + 0.001 );
}

float3 SpecularGGX(float Roughness, float3 SpecularColor, BxDFContext Context, half NoL, FAreaLight AreaLight)
{
    float3 o = 0.f;

    float a2 = Pow4(Roughness);
    

    float NDF = D_GGX(a2, Context.NoH);
    float Vis = Vis_SmithJointApprox(a2, Context.NoV, NoL);
    float F = UE_F_Schlick(SpecularColor, Context.VoH);

    o = NDF * F * Vis;

    return o;
}

float EnergyNormalization(inout float a2, float VoH, FAreaLight AreaLight)
{
    if (AreaLight.SphereSinAlphaSoft > 0)
    {
        // Modify Roughness
        a2 = saturate(a2 + Pow2(AreaLight.SphereSinAlphaSoft) / (VoH * 3.6 + 0.4));
    }

    float Sphere_a2 = a2;
    float Energy = 1;
    if (AreaLight.SphereSinAlpha > 0)
    {
        Sphere_a2 = New_a2(a2, AreaLight.SphereSinAlpha, VoH);
        Energy = a2 / Sphere_a2;
    }

    if (AreaLight.LineCosSubtended < 1)
    {
#if 1
        float LineCosTwoAlpha = AreaLight.LineCosSubtended;
        float LineTanAlpha = sqrt((1.0001 - LineCosTwoAlpha) / (1 + LineCosTwoAlpha));
        float Line_a2 = New_a2(Sphere_a2, LineTanAlpha, VoH);
        Energy *= sqrt(Sphere_a2 / Line_a2);
#else
        float LineCosTwoAlpha = AreaLight.LineCosSubtended;
        float LineSinAlpha = sqrt( 0.5 - 0.5 * LineCosTwoAlpha );
        float Line_a2 = New_a2( Sphere_a2, LineSinAlpha, VoH );
        Energy *= Sphere_a2 / Line_a2;
#endif
    }

    return Energy;
}

FDirectLighting DefaultLitBxDF(MaterialData materialData, float3 N, float3 V, float3 L, float Falloff, float NoL, FAreaLight AreaLight, FShadowTerms Shadow)
{
    FDirectLighting Lighting = (FDirectLighting)0;
    BxDFContext Context = (BxDFContext)0;

    Init(Context, N, V, L);
    float NoV, VoH, NoH;
    NoV = Context.NoV;
    VoH = Context.VoH;
    NoH = Context.NoH;
    
    //SphereMaxNoH(Context, AreaLight.SphereSinAlpha, true);
    Context.NoV = saturate(abs(Context.NoV) + 1e-5);

    float3 KD = (1 - UE_F_Schlick(materialData.SpecularColor, Context.VoH)) * (1 - materialData.Metallic);
    Lighting.Diffuse = Diffuse_Chan(materialData.DiffuseColor, Pow4(materialData.Roughness), NoV, NoL, VoH, NoH, GetAreaLightDiffuseMicroReflWeight(AreaLight));
    Lighting.Diffuse *= AreaLight.FalloffColor * Falloff  * KD;

    Lighting.Specular = SpecularGGX(materialData.Roughness, materialData.SpecularColor, Context, NoL, AreaLight);
    Lighting.Specular *= AreaLight.FalloffColor * Falloff ;
    
    FBxDFEnergyTerms energyTerm = ComputeFresnelEnergyTerms(GGXEnergyLookup(materialData.Roughness, NoV), materialData.SpecularColor);
     
    //Lighting.Diffuse *= ComputeEnergyPreservation(energyTerm);
    //Lighting.Specular *= ComputeEnergyConservation(energyTerm);

    return Lighting;
}

FDirectLighting IntegrateBxDF(MaterialData materialData, float3 N, float3 V, float3 L, float Falloff, float NoL,
    FAreaLight AreaLight, FShadowTerms Shadow)
{
    FDirectLighting o = (FDirectLighting) 0;

    o = DefaultLitBxDF(materialData, N, V, L, Falloff, NoL, AreaLight, Shadow);

    return o;
}

FDirectLighting EvaluateBxDF(MaterialData materialData, float3 N, float3 V, float3 L, float NoL, FShadowTerms Shadow)
{
    FAreaLight AreaLight;

    AreaLight.SphereSinAlpha = 0;
    AreaLight.SphereSinAlphaSoft = 0;
    AreaLight.LineCosSubtended = 1;
    AreaLight.FalloffColor = 1;
    AreaLight.IsRectAndDiffuseMicroReflWeight = 0;
    
    return IntegrateBxDF(materialData, N, V, L, 1, NoL, AreaLight, Shadow);
}