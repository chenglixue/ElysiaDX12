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
    NLOHMANN_JSON_SERIALIZE_ENUM(AOBlurQuality,
                                 {
                                 {AOBlurQuality::Low,
                                 "Low"},
                                 {AOBlurQuality::Middle,
                                 "Middle"},
                                 {AOBlurQuality::High,
                                 "High"},
                                 })

    enum class AODebugTarget : int
    {
        Importance,
        HIZMipmap,
        AO
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(AODebugTarget,
                                 {
                                 {AODebugTarget::Importance,
                                 "Importance"},
                                 {AODebugTarget::HIZMipmap,
                                 "HIZMipmap"},
                                 {AODebugTarget::AO,
                                 "AO"},
                                 })

    enum class AOType : int
    {
        Soft,
        Hard
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(AOType,
                                 {
                                 {AOType::Soft,
                                 "Soft"},
                                 {AOType::Hard,
                                 "Hard"},

                                 })

    struct AOParameter
    {
        bool IsEnableAO = true;
        float Radius = 1.f;
        float FadeRadius = 1.f;
        float FadeDistance = 1.f;
        float Bias = 0.025f;
        float IntensityMul = 1.f;
        float IntensityPow = 1.f;

        bool IsLerpAO = false;
        float TAALerpFactor = 0.5f;

        bool IsBlur = true;
        int BlurCount = 1;
        AOBlurQuality BlurQuality = AOBlurQuality::High;
        int BlurIntensity = 1;
        float Sharpness = 10.f;

        AODebugTarget debugTarget = AODebugTarget::AO;

        float HIZMipFactor = 1.f;
        int HIZMipmap = 1;

        bool IsTAA = true;
        float importanceIntensity = 2.f;
        float HIZRadius = 1.f;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AOParameter,
                                                    IsEnableAO,
                                                    Radius,
                                                    FadeRadius,
                                                    FadeDistance,
                                                    Bias,
                                                    IntensityMul,
                                                    IntensityPow,
                                                    IsLerpAO,
                                                    TAALerpFactor,
                                                    IsBlur,
                                                    BlurCount,
                                                    BlurQuality,
                                                    BlurIntensity,
                                                    Sharpness,
                                                    debugTarget,
                                                    HIZMipFactor,
                                                    HIZMipmap,
                                                    IsTAA,
                                                    importanceIntensity,
                                                    HIZRadius
        )
}