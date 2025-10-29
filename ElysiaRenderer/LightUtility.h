#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
	enum class LightType : uint8_t
	{
		None = 1 << 0,
		Dir = 1 << 1,
		Spot = 1 << 2,
		Point = 1 << 3
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
}