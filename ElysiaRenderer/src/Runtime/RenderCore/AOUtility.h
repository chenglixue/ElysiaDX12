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

    enum class AODebugTarget : int
    {
        Importance,
        HIZMipmap,
        AO
    };

    enum class AOType : int
    {
        Soft,
        Hard
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

        bool IsBlur = true;
        int BlurCount = 1;
        AOBlurQuality BlurQuality = AOBlurQuality::High;
        int BlurIntensity = 1;
        float Sharpness = 10.f;

        AODebugTarget debugTarget = AODebugTarget::AO;

        float HIZMipFactor = 1.f;
        int HIZMipmap = 1;

        bool IsImportance = true;
        float importanceIntensity = 2.f;
        float HIZRadius = 1.f;
    };
}