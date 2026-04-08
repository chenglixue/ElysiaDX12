#pragma once
#include "Programs/Helper.h"
#include "ThirdParty/ColorConversion.h"
#include "ThirdParty/FreesyncHDR.h"

enum ColorSpace : uint;

namespace ElysiaRenderer
{
    using namespace CAULDRON_DX12;
    enum class HDRQuality : UINT
    {
        Low = 0,
        High
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(HDRQuality,
                                 {
                                 {HDRQuality::Low,
                                 "Low"},
                                 {HDRQuality::High,
                                 "High"},
                                 })

    enum class TonemapMode : UINT
    {
        Neutral = 0,
        LMP,
        AMD,
        ACESFilm,
        Uncharted2,
        DX11DSK
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(TonemapMode,
                                 {
                                 {TonemapMode::Neutral,
                                 "Neutral"},
                                 {TonemapMode::LMP,
                                 "LMP"},
                                 {TonemapMode::AMD,
                                 "AMD"},
                                 {TonemapMode::ACESFilm,
                                 "ACESFilm"},
                                 {TonemapMode::Uncharted2,
                                 "Uncharted2"},
                                 {TonemapMode::DX11DSK,
                                 "DX11DSK"}
                                 })

    struct HDRParameter
    {
        bool IsUseHDR = true;
        HDRQuality HDRLevel = HDRQuality::High;
        TonemapMode tonemapMode = TonemapMode::LMP;
        ColorSpace colorSpace = ColorSpace::ColorSpace_REC709;
        CAULDRON_DX12::DisplayMode displayMode = CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR;
        float localExposure = 1.f;
        bool bShoulder;
        // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
        float SoftGap;
        // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
        float HdrMax;           // Maximum input value.
        float LpmExposure;      // Number of stops between 'hdrMax' and 18% mid-level on input.
        float Contrast;         // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
        float ShoulderContrast; // Shoulder shaping, 1.0 = no change (fast path).
        Vector3 Saturation;     // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
        Vector3 Crosstalk;      // One channel must be 1.0, the rest can be <= 1.0 but not zero.
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HDRParameter,
                                                    IsUseHDR,
                                                    HDRLevel,
                                                    tonemapMode,
                                                    colorSpace,
                                                    displayMode,
                                                    localExposure,
                                                    bShoulder,
                                                    SoftGap,
                                                    HdrMax,
                                                    LpmExposure,
                                                    Contrast,
                                                    ShoulderContrast,
                                                    Saturation,
                                                    Crosstalk)

    /*enum class DisplayMode : UINT
    {
        DISPLAYMODE_SDR = 0,
        DISPLAYMODE_FSHDR_Gamma22,
        DISPLAYMODE_FSHDR_SCRGB,
        DISPLAYMODE_HDR10_2084,
        DISPLAYMODE_HDR10_SCRGB
    };*/
}