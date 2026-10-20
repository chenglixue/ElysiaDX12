#pragma once
#include "Programs/public/Helper.h"

#include <cmath>
#include <vector>

namespace ElysiaRenderer
{
    // UE r.EyeAdaptation.VisualizeDebugType: scene after tonemap / percentile heatmap / luminance.
    enum class AutoExposureVisualizeDebugType : UINT
    {
        SceneColor = 0,
        HistogramDebug = 1,
        Luminance = 2
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(AutoExposureVisualizeDebugType,
                                 {
                                 {AutoExposureVisualizeDebugType::SceneColor, "SceneColor"},
                                 {AutoExposureVisualizeDebugType::HistogramDebug, "HistogramDebug"},
                                 {AutoExposureVisualizeDebugType::Luminance, "Luminance"}
                                 })

    // Shared by C++ dispatch/buffer size and HLSL via ShaderPass.Environment.SetDefine.
    constexpr int kAutoExposureHistogramSize = 64;

    // Pixel layout of the VisualizeHDR overlay. Keep in sync with
    // private/AutoExposureCommon.hlsli / CS_AutoExposureVisualize.hlsl (UE PostProcessVisualizeHDR.usf).
    namespace AutoExposureVisualize
    {
        constexpr int HistogramMarginX = 64;
        constexpr int HistogramHeight = 128;
        constexpr int HistogramMarginBottom = 32;
        constexpr int HistogramOuterBorder = 4;
        constexpr int HistogramTickInsetX = 8;
        constexpr int HistogramTickInsetWidth = 20;
        constexpr int MeterHalfSize = 10;
        constexpr int StatsOriginX = 30;
        constexpr int StatsOriginY = 28;
        constexpr int StatsLineHeight = 14;
        constexpr int StatsColumnWidth = 250;
    }

    // One key of UE's AutoExposureBiasCurve. X is scene EV100, Y is extra stops.
    // Linear between keys, constant outside. That is UE RCIM_Linear + RCCE_Constant.
    struct AutoExposureCompensationCurveKey
    {
        float ev100 = 0.0f;
        float bias = 0.0f;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AutoExposureCompensationCurveKey, ev100, bias)

    // UE bakes the curve to a 64-wide LUT over EV100 [-10, 20].
    // LuminanceMax is 1 here, so scene EV100 = log2(averageLuminance / middleGrey).
    constexpr int kAutoExposureCompensationCurveSamples = 64;
    constexpr float kAutoExposureCompensationCurveMinEV = -10.0f;
    constexpr float kAutoExposureCompensationCurveMaxEV = 20.0f;
    constexpr int kAutoExposureCompensationCurveMaxKeys = 32;
    static_assert(kAutoExposureCompensationCurveSamples % 4 == 0,
                  "Curve LUT is uploaded as float4s");

    // curve_test04.uasset FloatCurve. Every key is RCIM_Linear, so time and bias are enough.
    inline std::vector<AutoExposureCompensationCurveKey> DefaultExposureCompensationCurveKeys()
    {
        return {
            {-10.0f, -2.75f},
            {-6.0f, -2.75f},
            {-2.0f, -1.9f},
            {0.0f, -1.5f},
            {6.0f, -1.5f},
            {11.0f, -1.25f},
            {13.0f, -0.25f},
            {15.0f, 0.5f},
            {16.0f, 3.0f},
        };
    }

    // UE-style Histogram Auto Exposure. ComputeExposure adapts toward the
    // target scale in f-stops using Speed Up / Speed Down.
    struct AutoExposureParameter
    {
        bool enable = true;
        // Bake last frame's exposure into Scene Color. Off keeps metering and tonemap,
        // but does not premultiply lighting, sky, or TAA history.
        bool enablePreExposure = true;

        // Fraction of histogram mass to skip from dark / keep up to from dark (UE Low/High Percent as 0..1).
        // Defaults match UE 4.25+ Histogram Low 10% / High 90%.
        float lowPercent = 0.10f;
        float highPercent = 0.90f;

        // Histogram range. Same knob as UE HistogramLogMin/Max.
        // With UE r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange (LuminanceMax=1),
        // EV100 == log2(luminance). Defaults match that path: -10 .. 20.
        float minLogLuminance = -10.0f;
        float maxLogLuminance = 20.0f;

        // Clamp on percentile average luminance before mapping to middle grey.
        // Same knob as UE Min/Max Brightness. Stored linear; UI/overlay use EV100
        // (LuminanceMax=1). Defaults match UE extended range: EV100 -10 .. 20.
        float minAverageLuminance = 1.0f / 1024.0f; // exp2(-10)
        float maxAverageLuminance = 1048576.0f;     // exp2(20)

        // Constant extra stops. Applied after adaptation, so the slider is immediate.
        // scale *= exp2(exposureCompensation).
        float exposureCompensation = 0.0f;

        // UE AutoExposureBiasCurve. Off, or on with no keys, multiplies by 1.
        // Indexed by this frame's metered scene EV100, then eased through adaptation.
        // Default keys are curve_test04.uasset.
        bool exposureCompensationCurveEnable = true;
        std::vector<AutoExposureCompensationCurveKey> exposureCompensationCurveKeys =
            DefaultExposureCompensationCurveKeys();

        // Middle grey target (UE 4.25+: 18%).
        float middleGrey = 0.18f;

        // Temporal eye adaptation, in f-stops per second.
        // UE defaults: Speed Up 3 (dark -> bright), Speed Down 1 (bright -> dark).
        // ExponentialTransitionDistance is r.EyeAdaptation.ExponentialTransitionDistance (1.5 stops).
        float speedUp = 3.0f;
        float speedDown = 1.0f;
        float exponentialTransitionDistance = 1.5f;

        // UE ShowFlag.VisualizeHDR. Forces histogram generation even when enable is off.
        bool visualizeHDR = false;
        AutoExposureVisualizeDebugType visualizeDebugType = AutoExposureVisualizeDebugType::SceneColor;
        float visualizeLuminanceScale = 1.0f;
    };

    inline bool WantsAutoExposureHistogram(const AutoExposureParameter& p)
    {
        return p.enable || p.visualizeHDR;
    }

    inline bool UsePreExposure(const AutoExposureParameter& p)
    {
        return p.enable && p.enablePreExposure;
    }

    // UE EV100 <-> luminance with LuminanceMax = 1 (unitless / lens attenuation 0.78).
    inline float AutoExposureEV100ToLuminance(float ev100)
    {
        return exp2f(ev100);
    }

    inline float AutoExposureLuminanceToEV100(float luminance)
    {
        return log2f(luminance > 1e-8f ? luminance : 1e-8f);
    }

    inline void ComputeAutoExposureHistogramScaleBias(float minLogLuminance,
                                                      float maxLogLuminance,
                                                      float& histogramScale,
                                                      float& histogramBias)
    {
        const float minLog = minLogLuminance;
        const float maxLog = maxLogLuminance > (minLog + 1e-3f) ? maxLogLuminance : (minLog + 1e-3f);
        histogramScale = 1.0f / (maxLog - minLog);
        histogramBias = -minLog * histogramScale;
    }

    // UE GetExposureCompensationCurveLUTScaleBias. Maps EV100 onto texel centers.
    inline void ComputeAutoExposureCompensationCurveScaleBias(float& scale, float& bias)
    {
        constexpr float minX = kAutoExposureCompensationCurveMinEV;
        constexpr float maxX = kAutoExposureCompensationCurveMaxEV;
        constexpr float numSamples = static_cast<float>(kAutoExposureCompensationCurveSamples);
        scale = (numSamples - 1.0f) / (numSamples * (maxX - minX));
        bias = (0.5f / numSamples) - (minX * scale);
    }

    // Constant outside the first and last key. Between keys, linear in EV100.
    inline float EvalAutoExposureCompensationCurveBias(
        const std::vector<AutoExposureCompensationCurveKey>& keys,
        float ev100)
    {
        if (keys.empty())
            return 0.0f;

        const AutoExposureCompensationCurveKey* lower = &keys[0];
        const AutoExposureCompensationCurveKey* upper = &keys[0];
        for (const AutoExposureCompensationCurveKey& key : keys)
        {
            if (key.ev100 < lower->ev100)
                lower = &key;
            if (key.ev100 > upper->ev100)
                upper = &key;
        }

        if (ev100 <= lower->ev100)
            return lower->bias;
        if (ev100 >= upper->ev100)
            return upper->bias;

        const AutoExposureCompensationCurveKey* prev = lower;
        const AutoExposureCompensationCurveKey* next = upper;
        for (const AutoExposureCompensationCurveKey& key : keys)
        {
            if (key.ev100 <= ev100 && key.ev100 >= prev->ev100)
                prev = &key;
            if (key.ev100 >= ev100 && key.ev100 <= next->ev100)
                next = &key;
        }

        const float span = next->ev100 - prev->ev100;
        if (span <= 1e-6f)
            return next->bias;
        const float t = (ev100 - prev->ev100) / span;
        return prev->bias + (next->bias - prev->bias) * t;
    }

    // UE CreateCurveLUT: texel i stores exp2(bias at that EV100). Disabled curve is 1.
    inline void BakeAutoExposureCompensationCurve(const AutoExposureParameter& parameter,
                                                  float* outScales,
                                                  int sampleCount)
    {
        const int count = sampleCount < kAutoExposureCompensationCurveSamples
                              ? sampleCount
                              : kAutoExposureCompensationCurveSamples;
        const bool flat = !parameter.exposureCompensationCurveEnable ||
                          parameter.exposureCompensationCurveKeys.empty();
        const float minX = kAutoExposureCompensationCurveMinEV;
        const float maxX = kAutoExposureCompensationCurveMaxEV;
        const float denom = static_cast<float>(kAutoExposureCompensationCurveSamples - 1);
        for (int i = 0; i < count; ++i)
        {
            if (flat)
            {
                outScales[i] = 1.0f;
                continue;
            }
            const float ev100 = minX + (maxX - minX) * (static_cast<float>(i) / denom);
            outScales[i] = exp2f(EvalAutoExposureCompensationCurveBias(
                parameter.exposureCompensationCurveKeys,
                ev100));
        }
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AutoExposureParameter,
                                                    enable,
                                                    lowPercent,
                                                    highPercent,
                                                    minLogLuminance,
                                                    maxLogLuminance,
                                                    minAverageLuminance,
                                                    maxAverageLuminance,
                                                    exposureCompensation,
                                                    exposureCompensationCurveEnable,
                                                    exposureCompensationCurveKeys,
                                                    middleGrey,
                                                    speedUp,
                                                    speedDown,
                                                    exponentialTransitionDistance,
                                                    visualizeHDR,
                                                    visualizeDebugType,
                                                    visualizeLuminanceScale)
}
