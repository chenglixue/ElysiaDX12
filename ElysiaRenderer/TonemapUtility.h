#pragma once
#include "Helper.h"
#include "stdafx.h"

namespace ElysiaRenderer
{
	enum class HDRQuality : UINT
	{
		Low = 0,
		High
	};

	enum class TonemapMode : UINT
	{
		None = 0,
		Neutral
	};

	enum class DisplayMode : UINT
	{
		DISPLAYMODE_SDR = 0,
		DISPLAYMODE_FSHDR_Gamma22,
		DISPLAYMODE_FSHDR_SCRGB,
		DISPLAYMODE_HDR10_2084,
		DISPLAYMODE_HDR10_SCRGB
	};
}