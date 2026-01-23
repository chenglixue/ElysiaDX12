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
        int SampleCount = 16;
        int SampleStepCount = 4;
        float Radius = 1.f;
        float FadeRadius = 1.f;
        float FadeDistance = 1.f;
        float Bias = 0.025f;
        float IntensityMul = 1.f;
        float IntensityPow = 1.f;

        bool IsLerpAO = false;
        float AOLerpFactor = 0.1f;
        float TAALerpFactor = 0.5f;

        AOBlurQuality BlurQuality = AOBlurQuality::High;
        float BlurIntensity = 1.f;
        float Sharpness = 10.f;
    };
}