#ifndef LIGHT
#define LIGHT

#pragma once

#include "SharedCommon.hlsli"



LightData GetMainLight(Light light)
{
    LightData o = (LightData) 0;

    o.color = light.m_lightColor.rgb;
    o.intensity = light.m_intensity;
    o.direction = light.m_lightDir.rgb;
    o.toLight = -o.direction;
    o.lightAttenuation = 1;
    o.position = light.m_lightPos.rgb;


    return o;
}

#endif