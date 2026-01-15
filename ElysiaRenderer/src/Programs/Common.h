#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
    inline Vector4 GetScreenSize(Vector2 screenSize)
    {
        return Vector4(screenSize.x, screenSize.y, 1.f / screenSize.x, 1.f / screenSize.y);
    }

    inline Vector4 GetScreenSize(UINT width, UINT height)
    {
        return Vector4(width, height, 1.f / static_cast<float>(width), 1.f / static_cast<float>(height));
    }

    inline Vector4 GetScreenSize(UINT64 width, UINT64 height)
    {
        return Vector4(width, height, 1.f / static_cast<float>(width), 1.f / static_cast<float>(height));
    }

    inline Vector4 GetScreenSize(float width, float height)
    {
        return Vector4(width, height, 1.f / width, 1.f / height);
    }

#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
}