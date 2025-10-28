#pragma once

float3 GetNormal(float3 normalTS, float3x3 TBN, float normalIntensity = 1.f, bool isNormalized = true)
{
    normalTS = normalTS * 2.f - 1.f;
    normalTS.xy *= normalIntensity;
    float3 WorldNormal = isNormalized ? normalize(mul(TBN, normalTS)) : mul(TBN, normalTS);
    
    return WorldNormal;
} 