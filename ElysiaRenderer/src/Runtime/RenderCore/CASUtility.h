#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    struct SharpenParameter
    {
        bool enable;
        float sharpen;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SharpenParameter, enable, sharpen)
}