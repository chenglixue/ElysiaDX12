#ifndef SHADING_MODEL_H
#define SHADING_MODEL_H

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
    float3 Transmission;
};

float New_a2(float a2, float SinAlpha, float VoH)
{
    return a2 + 0.25 * SinAlpha * (3.0 * sqrt(a2) + SinAlpha) / (VoH + 0.001);
    //return a2 + 0.25 * SinAlpha * ( saturate(12 * a2 + 0.125) + SinAlpha ) / ( VoH + 0.001 );
    //return a2 + 0.25 * SinAlpha * ( a2 * 2 + 1 + SinAlpha ) / ( VoH + 0.001 );
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
        float LineSinAlpha = sqrt(0.5 - 0.5 * LineCosTwoAlpha);
        float Line_a2 = New_a2(Sphere_a2, LineSinAlpha, VoH);
        Energy *= Sphere_a2 / Line_a2;
#endif
    }

    return Energy;
}

float3 SpecularGGX(float Roughness,
                   float3 SpecularColor,
                   BxDFContext Context,
                   half NoL,
                   FAreaLight AreaLight)
{
    float3 o = 0.f;

    float a2 = Pow4(Roughness);

    float NDF = D_GGX(a2, Context.NoH);
    float Vis = Vis_SmithJointApprox(a2, Context.NoV, NoL);
    float F = UE_F_Schlick(SpecularColor, Context.VoH);

    o = NDF * F * Vis;

    return o;
}

FDirectLighting DefaultLitBxDF(FDecodeGBufferData GBufferData,
                               float3 N,
                               float3 V,
                               float3 L,
                               float Falloff,
                               float NoL,
                               FAreaLight AreaLight,
                               FShadowTerms Shadow)
{
    FDirectLighting Lighting = (FDirectLighting)0;
    BxDFContext Context = (BxDFContext)0;

    Init(Context, N, V, L);
    float NoV, VoH, NoH;
    NoV = Context.NoV;
    VoH = Context.VoH;
    NoH = Context.NoH;
    Context.NoH = max(0.f, dot(N, normalize(V + L)));

    //SphereMaxNoH(Context, AreaLight.SphereSinAlpha, true);
    Context.NoV = saturate(abs(NoV) + 1e-5);

    float3 KD = 1 - UE_F_Schlick(GBufferData.SpecularColor, Context.VoH);
    Lighting.Diffuse = Diffuse_Lambert(GBufferData.DiffuseColor);
    Lighting.Diffuse *= AreaLight.FalloffColor * Falloff * NoL * KD;

    Lighting.Specular = SpecularGGX(GBufferData.Roughness,
                                    GBufferData.SpecularColor,
                                    Context,
                                    NoL,
                                    AreaLight);
    Lighting.Specular *= AreaLight.FalloffColor * Falloff * NoL;

    FBxDFEnergyTerms energyTerm = ComputeFresnelEnergyTerms(
        GGXEnergyLookup(GBufferData.Roughness, Context.NoV),
        GBufferData.SpecularColor);

    // Lighting.Diffuse *= ComputeEnergyPreservation(energyTerm);
    // Lighting.Specular *= ComputeEnergyConservation(energyTerm);

    return Lighting;
}

float fresnelReflectance(float3 H, float3 V, float F0)
{
    float base = 1.0 - dot(V, H);
    float exponential = pow(base, 5.0);
    return exponential + F0 * (1.0 - exponential);
}

FDirectLighting PreintegratedSkinBxDF(FDecodeGBufferData GBufferData,
                                      float3 PosWS,
                                      float3 N,
                                      float3 V,
                                      float3 L,
                                      float Falloff,
                                      float NoL,
                                      FAreaLight AreaLight,
                                      FShadowTerms Shadow)
{
    FDirectLighting Lighting = DefaultLitBxDF(GBufferData, N, V, L, Falloff, NoL, AreaLight, Shadow);

    float3 subsurfaceColor = GBufferData.CustomData.rgb;
    float curvature = GBufferData.CustomData.a;

    float3 PreintegratedBRDF = SampleTexture2D_LOD(g_PreIntegrateSSSLUTIndex,
                                                   float2(saturate(dot(N, L) * 0.5f + 0.5f), curvature),
                                                   ClampLinearSampler,
                                                   0).rgb;
    Lighting.Transmission = AreaLight.FalloffColor * Falloff * PreintegratedBRDF * subsurfaceColor;

    // BxDFContext Context = (BxDFContext)0;
    // Init(Context, N, V, L);
    // float3 H = normalize(V + L);
    //
    // float UnclampedNoL = dot(N, L);
    // float curve = length(fwidth(N)) / length(fwidth(PosWS));
    // curve *= g_CurveScale;
    // curve = saturate(curve + 0.3f);
    // float3 SSSDiffuse = SampleTexture2D(g_PreIntegrateSSSLUTIndex,
    //                                     float2(UnclampedNoL * 0.5f + 0.5f, curve),
    //                                     ClampLinearSampler);
    // Lighting.Diffuse = (SSSDiffuse) * GBufferData.DiffuseColor;
    //
    // float NDF = SampleTexture2D(g_PreIntegrateSSSNDFLUTIndex,
    //                             float2(Context.NoH * 0.5 + 0.5f, GBufferData.Roughness),
    //                             ClampLinearSampler);
    // NDF = pow(2.f * NDF, 10.f);
    // float F = fresnelReflectance(H, V, GBufferData.SpecularColor);
    // float G = 1.0 / (4.0 * Context.VoH * Context.VoH);
    // Lighting.Specular = NDF * F * G * AreaLight.FalloffColor * Falloff * max(0.f, NoL);

    return Lighting;
}

FDirectLighting IntegrateBxDF(FDecodeGBufferData GBufferData,
                              float3 PosWS,
                              float3 N,
                              float3 V,
                              float3 L,
                              float Falloff,
                              float NoL,
                              FAreaLight AreaLight,
                              FShadowTerms Shadow)
{
    FDirectLighting o = (FDirectLighting)0;

    if (GBufferData.ShadingModelID == Shading_Model_ID_Unlit)
    {

    }
    else if (GBufferData.ShadingModelID == Shading_Model_ID_Default_Lit)
    {
        o = DefaultLitBxDF(GBufferData, N, V, L, Falloff, NoL, AreaLight, Shadow);
    }
    else if (GBufferData.ShadingModelID == Shading_Model_ID_Preintegrated_Skin)
    {
        o = PreintegratedSkinBxDF(GBufferData, PosWS, N, V, L, Falloff, NoL, AreaLight, Shadow);
    }

    return o;
}

FDirectLighting EvaluateBxDF(FDecodeGBufferData GBufferData,
                             float3 PosWS,
                             float3 N,
                             float3 V,
                             float3 L,
                             float NoL,
                             FShadowTerms Shadow)
{
    FAreaLight AreaLight;

    AreaLight.SphereSinAlpha = 0;
    AreaLight.SphereSinAlphaSoft = 0;
    AreaLight.LineCosSubtended = 1;
    AreaLight.FalloffColor = 1;
    AreaLight.IsRectAndDiffuseMicroReflWeight = 0;

    return IntegrateBxDF(GBufferData, PosWS, N, V, L, 1, NoL, AreaLight, Shadow);
}

#endif