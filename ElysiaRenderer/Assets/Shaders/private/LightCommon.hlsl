#ifndef LIGHT_COMMON_H
#define LIGHT_COMMON_H

#pragma once

#include "SharedCommon.hlsli"
#include "ShadingModel.hlsl"
#include "LightAccumulator.hlsl"
#include "AmbientCubemap.hlsl"
#include "ShadowCommon.hlsl"
#include "Light.hlsl"

struct FLightingSplit
{
    float4 DiffuseLighting;
    float4 SpecularLighting;
};

FLightAccumulator AccumulateDynamicLighting(FInputParams inputData,
                                            FDecodeGBufferData GBufferData,
                                            LightData lightData,
                                            float AO)
{
    FLightAccumulator o = (FLightAccumulator)0;
    FShadowTerms Shadow = (FShadowTerms)0;

    float3 V = -inputData.ScreenVector;
    float3 N = GBufferData.WorldNormal;
    float3 L = lightData.toLight;
    float3 MaskedLightColor = lightData.color * lightData.intensity;

    // float shadow = SunShadowVisibility(inputData.PositionWS,
    //                                    inputData.ScreenUV,
    //                                    inputData.ScreenSize,
    //                                    g_ShadowRadius,
    //                                    shadowSize,
    //                                    shadowMatrix,
    //                                    inputData.SobelSqeuence);
    float shadow = SampleTexture2D(g_ShadowMaskTexIndex, inputData.ScreenUV, ClampLinearSampler);
    if (!g_EnableShadow)
        shadow = 1;
    Shadow.SurfaceShadow = AO * shadow;

    FDirectLighting directLight = (FDirectLighting)0;
    float NoL = saturate(dot(N, L));
    directLight = EvaluateBxDF(GBufferData, inputData.PositionWS, N, V, L, NoL, Shadow);

    LightAccumulator_AddSplit(o,
                              directLight.Diffuse,
                              directLight.Specular,
                              directLight.Diffuse,
                              Shadow.SurfaceShadow * MaskedLightColor);
    LightAccumulator_AddSplit(o,
                              directLight.Transmission,
                              0,
                              directLight.Transmission,
                              Shadow.SurfaceShadow * MaskedLightColor);
    return o;
}

FLightingSplit GetLightAccumulator_ResultSplit(FLightAccumulator LightAccumulator)
{
    float4 RetDiffuse = 0.f;
    float4 RetSpecular = 0.f;

    RetDiffuse.rgb = LightAccumulator.TotalLightDiffuse;
    RetSpecular.rgb = LightAccumulator.TotalLightSpecular;

    FLightingSplit o = (FLightingSplit)0;
    o.DiffuseLighting = RetDiffuse;
    o.SpecularLighting = RetSpecular;

    return o;
}

FLightingSplit GetDynamicLightingSplit(FInputParams inputData,
                                       FDecodeGBufferData GBufferData,
                                       LightData lightData,
                                       float AO)
{
    FLightingSplit o = (FLightingSplit)0;

    FLightAccumulator lightAccumulator = AccumulateDynamicLighting(
        inputData,
        GBufferData,
        lightData,
        AO);
    o = GetLightAccumulator_ResultSplit(lightAccumulator);

    return o;
}

float4 GetDynamicLighting(FInputParams inputData,
                          FDecodeGBufferData GBufferData,
                          LightData lightData,
                          float AO)
{
    float4 o = 0.f;

    FLightingSplit lighting = (FLightingSplit)0;
    lighting = GetDynamicLightingSplit(inputData, GBufferData, lightData, AO);

    o += lighting.DiffuseLighting;
    o += lighting.SpecularLighting;
    o.a = GBufferData.Opacity;

    return (o);
}

#endif