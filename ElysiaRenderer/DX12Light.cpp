#include "DX12Light.h"

namespace ElysiaRenderer
{
	DX12Light::DX12Light(XMFLOAT4 lightColor, XMVECTOR lightDir, float intensity)
		: m_lightType(LightType::None), m_lightColor(lightColor), m_lightDir(lightDir), m_lightIntensity(intensity)
	{
	}

	DX12DirectionLight::DX12DirectionLight(XMFLOAT4 lightColor, XMVECTOR lightDir, float intensity)
		: DX12Light(lightColor, lightDir, intensity), m_lightType(LightType::Dir)
	{
	}

	LightData DX12DirectionLight::CreateLightData()
	{
		LightData o{};

		o.m_lightColor = m_lightColor;
		o.m_lightDir = m_lightDir;
		o.m_intensity = m_lightIntensity;
		o.m_lightPos = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
		o.m_falloffStart = FLT_MAX;
		o.m_falloffEnd = FLT_MAX;
		o.m_spotPower = FLT_MAX;

		return o;
	}
}