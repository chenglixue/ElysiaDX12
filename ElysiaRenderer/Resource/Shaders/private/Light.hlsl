#pragma once

#include "SharedCommon.hlsli"



LightData GetMainLight(Light light)
{
    LightData o = (LightData) 0;

    o.color = light.m_lightColor;
    o.intensity = light.m_intensity;
    o.direction = normalize(light.m_lightDir);
    o.toLight = -o.direction;
    o.lightAttenuation = 1;
    o.position = FLT_MAX;


    return o;
}