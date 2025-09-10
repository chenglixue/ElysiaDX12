#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	enum class LightType : uint8_t
	{
		None	= 1 << 0,
		Dir		= 1 << 1,
		Spot	= 1 << 2,
		Point	= 1 << 3
	};

	struct LightData
	{
		// 16
		XMFLOAT4	m_lightColor;

		// 16
		XMFLOAT4	m_lightDir;

		// 16
		XMFLOAT4	m_lightPos;

		// 16
		float		m_falloffStart;
		float		m_falloffEnd;
		float		m_spotPower;
		float		m_intensity;
	};

	class DX12Light
	{
	public:
		DX12Light() = default;
		DX12Light(XMFLOAT3 lightColor, XMFLOAT3 lightDir, float intensity);
		DX12Light(const DX12Light& rhs) = default;
		DX12Light& operator=(const DX12Light& rhs) = default;
		DX12Light(DX12Light&& rhs) = default;
		~DX12Light() = default;

		const LightType& GetLightType()
		{
			return m_lightType;
		}
		const XMFLOAT3& GetLightColor()
		{
			return m_lightColor;
		}
		XMFLOAT3& GetLightDir()
		{
			return m_lightDir;
		}
		float GetLightIntensity()
		{
			return m_lightIntensity;
		}

		virtual LightData CreateLightData() = 0;

	protected:
		XMFLOAT3 m_lightColor;
		XMFLOAT3 m_lightDir;
		float m_lightIntensity;
		XMFLOAT3 m_lightPos;
		LightType m_lightType = LightType::None;
	};

	class DX12DirectionLight : public DX12Light
	{
	public:
		DX12DirectionLight() = default;
		DX12DirectionLight(XMFLOAT3 lightColor, XMFLOAT3 lightDir, float intensity);
		DX12DirectionLight(const DX12DirectionLight& rhs) = default;
		DX12DirectionLight& operator=(const DX12DirectionLight& rhs) = default;
		DX12DirectionLight(DX12DirectionLight&& rhs) = default;
		~DX12DirectionLight() = default;

		virtual LightData CreateLightData() override;

	private:
		LightType m_lightType = LightType::Dir;
	};
}