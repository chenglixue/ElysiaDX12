#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
	struct AOParameter
	{
		bool IsEnableAO = true;
		UINT SampleCount = 32;
		float Radius = 1.f;
		float IntensityMul = 1.f;
		float IntensityPow = 1.f;
	};
}