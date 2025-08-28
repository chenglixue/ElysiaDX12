#pragma once

struct FLightAccumulator
{
    float3 TotalLight;

    // only used for alpha, which needs to keep specular and alpha separate since specular needs to multiply by 1/opacity to compensate for alpha blending
    // assumed to be compiled out otherwise
    float3 TotalLightDiffuse;
    float3 TotalLightSpecular;
};

void LightAccumulator_AddSplit(inout FLightAccumulator In, float3 DiffuseTotalLight, float3 SpecularTotalLight, float3 ScatterableLight, float3 CommonMultiplier,
    const bool bNeedsSeparateSubsurfaceLightAccumulation = false)
{
    In.TotalLight += (DiffuseTotalLight + SpecularTotalLight) * CommonMultiplier;
    In.TotalLightDiffuse += DiffuseTotalLight * CommonMultiplier;
    In.TotalLightSpecular += SpecularTotalLight * CommonMultiplier;
}