#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    struct BloomParameter
    {
        bool enable = true;
        float radius = 1.f;
        float intensity = 1.f;
        int mipmap = 0;
    };
}