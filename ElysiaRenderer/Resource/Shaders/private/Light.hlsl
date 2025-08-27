#pragma once

#include "SharedCommon.hlsli"



Light GetMainLight(LightData light)
{
    Light o = (Light)0;

    o.color = light.m_lightColor;
    o.intensity = light.m_intensity;
    o.direction = light.m_lightDir;
    o.lightAttenuation = 1;
    o.position = FLT_MAX;


    return o;
}