#pragma once

#include "SharedCommon.hlsli"

void LightAccumulator_AddSplit(inout FLightAccumulator In, float3 DiffuseTotalLight, float3 SpecularTotalLight, float3 ScatterableLight, float3 CommonMultiplier,
    const bool bNeedsSeparateSubsurfaceLightAccumulation = false)
{
    In.TotalLight += (DiffuseTotalLight + SpecularTotalLight) * CommonMultiplier;
    In.TotalLightDiffuse += DiffuseTotalLight * CommonMultiplier;
    In.TotalLightSpecular += SpecularTotalLight * CommonMultiplier;
}

void LightAccumulator_Add(inout FLightAccumulator In, float3 TotalLight, float3 ScatterableLight, float3 CommonMultiplier, const bool bNeedsSeparateSubsurfaceLightAccumulation)
{
    LightAccumulator_AddSplit(In, TotalLight, 0.0f, ScatterableLight, CommonMultiplier, bNeedsSeparateSubsurfaceLightAccumulation);
}

//
// compute final value to store in the MRT0
// @retrun RGB:SceneColor Specular and Diffuse, A:Non Specular SceneColor Luminance
float3 GetLightAccumulator_Result(FLightAccumulator LightAccumulator)
{
    float3 o = 0.f;

    o = LightAccumulator.TotalLight;
    
    return o;
}