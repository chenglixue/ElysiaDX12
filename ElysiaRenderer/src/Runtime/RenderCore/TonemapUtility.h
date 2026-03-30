#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
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

    /*enum class DisplayMode : UINT
    {
        DISPLAYMODE_SDR = 0,
        DISPLAYMODE_FSHDR_Gamma22,
        DISPLAYMODE_FSHDR_SCRGB,
        DISPLAYMODE_HDR10_2084,
        DISPLAYMODE_HDR10_SCRGB
    };*/
}