#pragma once

#include "SharedCommon.hlsli"
#include "ShadingModel.hlsl"
#include "LightAccumulator.hlsl"
//#include "AmbientCubemap.hlsl"

struct FLightingSplit
{
    float4 DiffuseLighting;
    float4 SpecularLighting;
};

FLightAccumulator AccumulateDynamicLighting(FInputParams inputData, MaterialData materialData, LightData lightData)
{
    FLightAccumulator o = (FLightAccumulator) 0;
    FShadowTerms Shadow = (FShadowTerms) 0;
    
    float3 V = -inputData.ScreenVector;
    float3 N = materialData.WorldNormal;
    N = inputData.NormalWS;
    float3 L = lightData.toLight;
    float3 MaskedLightColor = lightData.color * lightData.intensity;
    Shadow.SurfaceShadow = 1;
    
    FDirectLighting directLight = (FDirectLighting) 0;
    float NoL = saturate(dot(N, L));
    directLight = EvaluateBxDF(materialData, N, V, L, NoL, Shadow);

    LightAccumulator_AddSplit(o, directLight.Diffuse, directLight.Specular, directLight.Diffuse, Shadow.SurfaceShadow * MaskedLightColor * PI);
    
    return o;
}

FLightingSplit GetLightAccumulator_ResultSplit(FLightAccumulator LightAccumulator)
{
    float4 RetDiffuse = 0.f;
    float4 RetSpecular = 0.f;

    RetDiffuse.rgb = LightAccumulator.TotalLightDiffuse;
    RetSpecular.rgb = LightAccumulator.TotalLightSpecular;

    FLightingSplit o = (FLightingSplit) 0;
    o.DiffuseLighting = RetDiffuse;
    o.SpecularLighting = RetSpecular;

    return o;
}

FLightingSplit GetDynamicLightingSplit(FInputParams inputData, MaterialData materialData, LightData lightData)
{
    FLightingSplit o = (FLightingSplit) 0;
    
    FLightAccumulator lightAccumulator = AccumulateDynamicLighting(inputData, materialData, lightData);
    o = GetLightAccumulator_ResultSplit(lightAccumulator);
    
    return o;
}

float4 GetDynamicLighting(FInputParams inputData, MaterialData materialData, LightData lightData)
{
    float4 o = 0.f;
    
    FLightingSplit lighting = (FLightingSplit) 0;
    lighting = GetDynamicLightingSplit(inputData, materialData, lightData);
    
    
    o += lighting.DiffuseLighting + lighting.SpecularLighting;
    //o += float4(GetIBL(inputData, materialData, lightData.toLight), 1.f);
    o.a = materialData.Opacity;
    
    return o;
}