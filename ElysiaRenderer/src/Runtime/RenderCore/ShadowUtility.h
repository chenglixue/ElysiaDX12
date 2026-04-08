#pragma once
#include "AOUtility.h"
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    enum class ShadowQuality : uint8_t
    {
        Low = 0,
        Middle = 1,
        High = 2,
        VeryHigh = 3
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(ShadowQuality,
                                 {
                                 {ShadowQuality::Low,
                                 "Low"},
                                 {ShadowQuality::Middle,
                                 "Medium"},
                                 {ShadowQuality::High,
                                 "High"},
                                 {ShadowQuality::VeryHigh,
                                 "VeryHigh"}
                                 })

    enum class ShadowType : uint8_t
    {
        Hard = 0,
        Soft = 1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ShadowType,
                                 {
                                 {ShadowType::Hard,
                                 "Hard"},
                                 {ShadowType::Soft,
                                 "Soft"}
                                 })

    struct ShadowParameter
    {
        ShadowType shadowType = ShadowType::Soft;
        ShadowQuality shadowQuality = ShadowQuality::VeryHigh;
        float shadowDepthBias = 0;
        float shadowSlopeDepthBias = 0;
        float shadowMaxSlopeDepthBias = 0;
        float shadowRadius = 1.f;
        bool EnableShadow = true;
        bool EnableTAA = true;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ShadowParameter,
                                                    shadowType,
                                                    shadowQuality,
                                                    shadowDepthBias,
                                                    shadowSlopeDepthBias,
                                                    shadowMaxSlopeDepthBias,
                                                    shadowRadius,
                                                    EnableShadow,
                                                    EnableTAA)
}