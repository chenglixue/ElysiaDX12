#pragma once

#include "Light.hlsli"

float GetAreaLightDiffuseMicroReflWeight(FAreaLight AreaLight)
{
    return asfloat(AreaLight.IsRectAndDiffuseMicroReflWeight >> 1);
}