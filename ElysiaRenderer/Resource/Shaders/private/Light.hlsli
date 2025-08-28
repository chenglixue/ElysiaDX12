#ifndef LIGHT_H
#define LIGHT_H

#define MAIN_LIGHT_NUM 1

struct LightData
{
    float3 color;
    float intensity;
    
    float lightAttenuation;
    
    float3 direction;
    float3 position;
    
    float3 toLight;
};

struct Light
{
    float4 m_lightColor;
    float4 m_lightDir;
    float4 m_lightPos;
    float m_falloffStart;
    float m_falloffEnd;
    float m_spotPower;
    float m_intensity;
};

struct FAreaLight
{
    float SphereSinAlpha;
    float SphereSinAlphaSoft;
    float LineCosSubtended;

    float3 FalloffColor;
    uint IsRectAndDiffuseMicroReflWeight;
};
#endif