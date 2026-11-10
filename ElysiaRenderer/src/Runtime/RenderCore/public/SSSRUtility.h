#pragma once
#include "Programs/public/DebugUtility.h"

namespace ElysiaRenderer
{
    // Nested target under DebugMode::SSSR. Append only — values are persisted.
    enum class SSSRDebugTarget : UINT
    {
        None = 0,
        HIZ = 1,
        TileClassify = 2,
        Intersection = 3,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(SSSRDebugTarget,
                                 {
                                 {SSSRDebugTarget::None, "None"},
                                 {SSSRDebugTarget::HIZ, "HIZ"},
                                 {SSSRDebugTarget::TileClassify, "TileClassify"},
                                 {SSSRDebugTarget::Intersection, "Intersection"}
                                 })

    // Sub-views for SSSRDebugTarget::TileClassify. Append only — values are persisted.
    enum class SSSRTileClassifyDebugView : UINT
    {
        Classification = 0, // invalid / env / shoot / copy-candidate
        CopyFlags = 1       // RGB = horizontal / vertical / diagonal copy on shoot rays
    };

    // Tooltip text for the Tile Classify debug view combo (items + current value).
    inline const char* GetSSSRTileClassifyDebugViewDescription(SSSRTileClassifyDebugView view)
    {
        switch (view)
        {
        case SSSRTileClassifyDebugView::Classification:
            return "Dark: sky / invalid pixel.\nBlue: falls back to environment map.\nRed: shoots a ray.\nYellow: valid pixel that needs a copy.";
        case SSSRTileClassifyDebugView::CopyFlags:
            return "On pixels that shoot a ray, RGB = copy horizontal / vertical / diagonal flags.\nDim yellow: copy candidates. Dim blue: environment fallback.";
        default:
            return nullptr;
        }
    }

    inline const char* GetSSSRDebugTargetDescription(SSSRDebugTarget target)
    {
        switch (target)
        {
        case SSSRDebugTarget::Intersection:
            return "Viewport shows only screen-space reflections.\nHits show reflected radiance. Misses and environment fallbacks are black.";
        default:
            return nullptr;
        }
    }

    NLOHMANN_JSON_SERIALIZE_ENUM(SSSRTileClassifyDebugView,
                                 {
                                 {SSSRTileClassifyDebugView::Classification, "Classification"},
                                 {SSSRTileClassifyDebugView::CopyFlags, "CopyFlags"}
                                 })

    struct SSSRParameter
    {
        bool enable = true;
        float roughnessThreshold = 0.4f;
        UINT samplesPerQuad = 1;
        UINT maxRaySteps = 128;
        float thickness = 0.5f;
        float intensity = 1.0f;
        int debugMip = 0;
        bool debugValidate = false;
        SSSRDebugTarget debugTarget = SSSRDebugTarget::None;
        SSSRTileClassifyDebugView tileClassifyDebugView = SSSRTileClassifyDebugView::Classification;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SSSRParameter,
                                                    enable,
                                                    roughnessThreshold,
                                                    samplesPerQuad,
                                                    maxRaySteps,
                                                    thickness,
                                                    intensity,
                                                    debugMip,
                                                    debugValidate,
                                                    debugTarget,
                                                    tileClassifyDebugView)

    inline UINT ClampSSSRSamplesPerQuad(int samples)
    {
        if (samples >= 4)
            return 4;
        if (samples >= 2)
            return 2;
        return 1;
    }

    inline UINT ClampSSSRMaxRaySteps(int steps)
    {
        if (steps < 1)
            return 1;
        if (steps > 1024)
            return 1024;
        return static_cast<UINT>(steps);
    }

    // Migrate legacy DebugMode::SSSRHIZ / SSSRTileClassify into DebugMode::SSSR + debugTarget.
    // Idempotent. Old enum numeric values remain for append-only JSON / ini compatibility.
    inline void NormalizeSSSRDebugMode(DebugMode& debugMode, SSSRParameter& sssr)
    {
        if (debugMode == DebugMode::SSSRHIZ)
        {
            debugMode = DebugMode::SSSR;
            sssr.debugTarget = SSSRDebugTarget::HIZ;
        }
        else if (debugMode == DebugMode::SSSRTileClassify)
        {
            debugMode = DebugMode::SSSR;
            sssr.debugTarget = SSSRDebugTarget::TileClassify;
        }
    }

    inline bool IsSSSRTileClassifyDebugActive(DebugMode debugMode, const SSSRParameter& sssr)
    {
        return (debugMode == DebugMode::SSSR && sssr.debugTarget == SSSRDebugTarget::TileClassify) ||
               debugMode == DebugMode::SSSRTileClassify;
    }

    inline bool IsSSSRIntersectionDebugActive(DebugMode debugMode, const SSSRParameter& sssr)
    {
        return debugMode == DebugMode::SSSR && sssr.debugTarget == SSSRDebugTarget::Intersection;
    }

    inline bool IsSSSRDebugActive(DebugMode debugMode, const SSSRParameter& sssr)
    {
        if (debugMode == DebugMode::SSSRHIZ || debugMode == DebugMode::SSSRTileClassify)
            return true;
        return debugMode == DebugMode::SSSR && sssr.debugTarget != SSSRDebugTarget::None;
    }
}
