#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
	inline Vector4 GetScreenSize(Vector2 screenSize)
	{
		return Vector4(screenSize.x, screenSize.y, 1.f / screenSize.x, 1.f / screenSize.y);
	}
}