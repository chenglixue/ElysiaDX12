#pragma once

float3 GetNormal(float3 normalTS, float3x3 TBN, bool isNormalized = true)
{
    normalTS = normalTS * 2.f - 1.f;
    float3 WorldNormal = isNormalized ? normalize(mul(normalTS, TBN)) : mul(normalTS, TBN);
    
    return WorldNormal;
}