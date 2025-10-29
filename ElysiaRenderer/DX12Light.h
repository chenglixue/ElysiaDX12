#pragma once
#include "LightUtility.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;
	using namespace ElysiaHelper;

	class DX12Light
	{
	public:
		DX12Light() = default;
		DX12Light(Vector3 lightColor, Vector3 lightDir, float intensity);
		DX12Light(const DX12Light& rhs) = default;
		DX12Light& operator=(const DX12Light& rhs) = default;
		DX12Light(DX12Light&& rhs) = default;
		~DX12Light() = default;

		LightType GetLightType() const;
		Vector3 GetLightColor() const;
		Vector3 GetLightDir() const;
		float GetLightIntensity() const;

		void SetLightColor(const Vector3& lightColor);
		void SetLightDir(const Vector3& lightDir);
		void SetLightIntensity(float lightIntensity);

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