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
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(SSSRDebugTarget,
                                 {
                                 {SSSRDebugTarget::None, "None"},
                                 {SSSRDebugTarget::HIZ, "HIZ"},
                                 {SSSRDebugTarget::TileClassify, "TileClassify"}
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

    NLOHMANN_JSON_SERIALIZE_ENUM(SSSRTileClassifyDebugView,
                                 {
                                 {SSSRTileClassifyDebugView::Classification, "Classification"},
                                 {SSSRTileClassifyDebugView::CopyFlags, "CopyFlags"}
                                 })

    struct SSSRParameter
    {
        float roughnessThreshold = 1;
        UINT samplesPerQuad = 1;
        int debugMip = 0;
        bool debugValidate = false;
        SSSRDebugTarget debugTarget = SSSRDebugTarget::None;
        SSSRTileClassifyDebugView tileClassifyDebugView = SSSRTileClassifyDebugView::Classification;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SSSRParameter,
                                                    roughnessThreshold,
                                                    samplesPerQuad,
                                                    debugMip,
                                                    debugValidate,
                                                    debugTarget,
                                                    tileClassifyDebugView)

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
}