#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
	enum class ShadowQuality : uint8_t
	{
		Low = 0,
		Middle = 1,
		High = 2,
		VeryHigh = 3
	};

	enum class ShadowType : uint8_t
	{
		Hard = 0,
		Soft = 1
	};
}