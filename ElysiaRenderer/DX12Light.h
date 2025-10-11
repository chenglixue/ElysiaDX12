#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;

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
		Vector4	m_lightColor;

		// 16
		Vector4	m_lightDir;

		// 16
		Vector4	m_lightPos;

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
		DX12Light(Vector3 lightColor, Vector3 lightDir, float intensity);
		DX12Light(const DX12Light& rhs) = default;
		DX12Light& operator=(const DX12Light& rhs) = default;
		DX12Light(DX12Light&& rhs) = default;
		~DX12Light() = default;

		LightType GetLightType() const noexcept
		{
			return m_lightType;
		}
		Vector3 GetLightColor() const noexcept
		{
			return m_lightColor;
		}
		Vector3 GetLightDir() const noexcept
		{
			return m_lightDir;
		}
		float GetLightIntensity() const noexcept
		{
			return m_lightIntensity;
		}

		void SetLightColor(const Vector3& lightColor)
		{
			m_lightColor = lightColor;
		}
		void SetLightDir(const Vector3& lightDir)
		{
			m_lightDir = lightDir;
		}
		void SetLightIntensity(float lightIntensity)
		{
			m_lightIntensity = lightIntensity;
		}

		virtual LightData CreateLightData() = 0;

	
		Vector3 m_lightColor;
		Vector3 m_lightDir;
		float m_lightIntensity;
		Vector3 m_lightPos;
	protected:
		LightType m_lightType = LightType::None;
	};

	class DX12DirectionLight : public DX12Light
	{
	public:
		DX12DirectionLight() = default;
		DX12DirectionLight(Vector3 lightColor, Vector3 lightDir, float intensity);
		DX12DirectionLight(const DX12DirectionLight& rhs) = default;
		DX12DirectionLight& operator=(const DX12DirectionLight& rhs) = default;
		DX12DirectionLight(DX12DirectionLight&& rhs) = default;
		~DX12DirectionLight() = default;

		virtual LightData CreateLightData() override;

	private:
		LightType m_lightType = LightType::Dir;
	};
}