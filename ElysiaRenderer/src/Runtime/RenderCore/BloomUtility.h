#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    enum class DebugDownOrUp
    {
        Down,
        Up
    };
    struct BloomParameter
    {
        bool enable = true;
        float radius = 1.f;
        float intensity = 1.f;

        int mipmap = 0;
        DebugDownOrUp debugMode = DebugDownOrUp::Down;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BloomParameter,
                                                    enable,
                                                    radius,
                                                    intensity,
                                                    mipmap)
}