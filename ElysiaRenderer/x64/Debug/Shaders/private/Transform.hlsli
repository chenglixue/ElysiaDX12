#pragma once

float3 GetNormal(float3 normalTS, float3x3 TBN, float normalIntensity = 1.f, bool isNormalized = true)
{
    normalTS = normalTS * 2.f - 1.f;
    normalTS.xy *= normalIntensity;
    normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
    float3 WorldNormal = isNormalized ? normalize(mul(normalTS, TBN)) : mul(normalTS, TBN);

    return WorldNormal;
}