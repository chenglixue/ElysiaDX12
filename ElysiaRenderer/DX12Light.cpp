#include "stdafx.h"
#include "Helper.h"
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

	LightType DX12Light::GetLightType() const
	{
		return m_lightType;
	}
	Vector3 DX12Light::GetLightColor() const 
	{
		return m_lightColor;
	}
	Vector3 DX12Light::GetLightDir() const 
	{
		return m_lightDir;
	}
	float DX12Light::GetLightIntensity() const 
	{
		return m_lightIntensity;
	}

	void DX12Light::SetLightColor(const Vector3& lightColor)
	{
		m_lightColor = lightColor;
	}
	void DX12Light::SetLightDir(const Vector3& lightDir)
	{
		m_lightDir = lightDir;
	}
	void DX12Light::SetLightIntensity(float lightIntensity)
	{
		m_lightIntensity = lightIntensity;
	}

	LightData DX12DirectionLight::CreateLightData()
	{
		LightData o{};

		o.m_lightColor = Vector4(m_lightColor.x, m_lightColor.y, m_lightColor.z, 1.f);
		o.m_lightDir = Vector4(m_lightDir.x, m_lightDir.y, m_lightDir.z, 0.f);
		o.m_intensity = m_lightIntensity;
		o.m_lightPos = Vector4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
		o.m_falloffStart = FLT_MAX;
		o.m_falloffEnd = FLT_MAX;
		o.m_spotPower = FLT_MAX;

		return o;
	}
}