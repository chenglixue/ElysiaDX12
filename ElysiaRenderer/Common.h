#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
	using namespace SimpleMath;

	inline Vector4 GetScreenSize(Vector2 screenSize)
	{
		return Vector4(screenSize.x, screenSize.y, 1.f / screenSize.x, 1.f / screenSize.y);
	}

	#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
}