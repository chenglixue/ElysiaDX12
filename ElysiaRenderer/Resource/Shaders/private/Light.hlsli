#ifndef LIGHT_H
#define LIGHT_H

#define MAIN_LIGHT_NUM 1

struct Light
{
    float3 color;
    float intensity;
    
    float lightAttenuation;
    
    float3 direction;
    float3 position;
};

struct LightData
{
    float4 m_lightColor;
    float4 m_lightDir;
    float4 m_lightPos;
    float m_falloffStart;
    float m_falloffEnd;
    float m_spotPower;
    float m_intensity;
};

struct FDeferredLightingSplit
{
    float4 DiffuseLighting;
    float4 SpecularLighting;
};
#endif