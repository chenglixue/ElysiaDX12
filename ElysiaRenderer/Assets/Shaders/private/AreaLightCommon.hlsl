#ifndef AREA_LIGHT_COMMON_H
#define AREA_LIGHT_COMMON_H

#pragma once

#include "Light.hlsli"

float GetAreaLightDiffuseMicroReflWeight(FAreaLight AreaLight)
{
    return asfloat(AreaLight.IsRectAndDiffuseMicroReflWeight >> 1);
}

#endif