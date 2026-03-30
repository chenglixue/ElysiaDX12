#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
    enum class DebugMode : UINT
    {
        None,
        AO,
        GIProbe,
        Normal,
        AABB,
        Bloom,
        Velocity,
        GI
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(DebugMode,
                                 {
                                 {DebugMode::None,
                                 "None"},
                                 {DebugMode::AO,
                                 "AO"},
                                 {DebugMode::GIProbe,
                                 "GIProbe"},
                                 {DebugMode::Normal,
                                 "Normal"},
                                 {DebugMode::AABB,
                                 "AABB"},
                                 {DebugMode::Bloom,
                                 "Bloom"},
                                 {DebugMode::Velocity,
                                 "Velocity"},
                                 {DebugMode::GI,
                                 "GI"}
                                 })
}