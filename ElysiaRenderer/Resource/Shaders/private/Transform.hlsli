#pragma once

float3 GetNormal(float3 normalTS, float3x3 TBN, bool isNormalized = true)
{
    normalTS = normalTS * 2. - 1.;
    float3 WorldNormal = select(isNormalized, normalize(mul(normalTS, TBN)), mul(normalTS, TBN));
    
    return WorldNormal;
}