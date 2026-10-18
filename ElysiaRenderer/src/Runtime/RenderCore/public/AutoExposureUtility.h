#pragma once
#include "Programs/public/Helper.h"

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

    // UE-style Histogram Auto Exposure. Temporal smoothing parameters are wired
    // through, but ComputeExposure still applies the target scale immediately.
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

        // Extra EV compensation: scale *= exp2(exposureCompensation).
        float exposureCompensation = 0.0f;

        // Middle grey target (UE 4.25+: 18%).
        float middleGrey = 0.18f;

        // Temporal eye adaptation, in f-stops per second. Not applied yet.
        // UE defaults: Speed Up 3, Speed Down 1.
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

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AutoExposureParameter,
                                                    enable,
                                                    lowPercent,
                                                    highPercent,
                                                    minLogLuminance,
                                                    maxLogLuminance,
                                                    minAverageLuminance,
                                                    maxAverageLuminance,
                                                    exposureCompensation,
                                                    middleGrey,
                                                    visualizeHDR,
                                                    visualizeDebugType,
                                                    visualizeLuminanceScale)
}
