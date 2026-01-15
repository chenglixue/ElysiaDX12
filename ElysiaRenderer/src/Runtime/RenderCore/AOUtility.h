#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    enum class AOBlurQuality : int
    {
        Low,
        Middle,
        High
    };

    struct AOParameter
    {
        bool IsEnableAO = true;
        UINT SampleCount = 32;
        float Radius = 1.f;
        float Bias = 0.025f;
        float IntensityMul = 1.f;
        float IntensityPow = 1.f;
        AOBlurQuality BlurQuality = AOBlurQuality::High;
        float BlurIntensity = 1.f;
        float Sharpness = 10.f;
    };
}