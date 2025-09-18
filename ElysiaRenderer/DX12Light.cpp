#include "DX12Light.h"

namespace ElysiaRenderer
{
	DX12Light::DX12Light(Vector3 lightColor, Vector3 lightDir, float intensity)
		: m_lightType(LightType::None), m_lightColor(lightColor), m_lightDir(lightDir), m_lightIntensity(intensity)
	{
	}

	DX12DirectionLight::DX12DirectionLight(Vector3 lightColor, Vector3 lightDir, float intensity)
		: DX12Light(lightColor, lightDir, intensity), m_lightType(LightType::Dir)
	{
	}

	LightData DX12DirectionLight::CreateLightData()
	{
		LightData o{};

		o.m_lightColor = Vector4(m_lightColor.x, m_lightColor.y, m_lightColor.z, 1.f);

		Vector3 temp;
		XMStoreFloat3(&temp, XMVector3Normalize(XMLoadFloat3(&m_lightDir)));
		o.m_lightDir = Vector4(temp.x, temp.y, temp.z, 0.f);
		o.m_intensity = m_lightIntensity;
		o.m_lightPos = Vector4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
		o.m_falloffStart = FLT_MAX;
		o.m_falloffEnd = FLT_MAX;
		o.m_spotPower = FLT_MAX;

		return o;
	}
}