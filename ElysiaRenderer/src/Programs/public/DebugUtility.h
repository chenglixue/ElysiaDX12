#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
    // Numeric values are persisted in ConfigCache / JSON. Append only.
    enum class DebugMode : UINT
    {
        None = 0,           // Viewport View Mode: Lit
        AO,                 // Buffer Visualization: AO
        GIProbe,
        Normal,             // Buffer Visualization: World Normal
        AABB,
        Bloom,
        Velocity,           // Buffer Visualization: Velocity
        GI,
        ShadowMask,
        Albedo,             // Buffer Visualization: Base Color
        Emission,           // Viewport View Mode: Unlit
        Metallic,
        Roughness,
        ShadowMap,
        Specular,
        WorldTangent,
        SceneDepth,
        Opacity,
        PreTonemapHDR,
        PostTonemapHDR,
        LightingOnly,
        ShadingModel
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(DebugMode,
                                 {
                                 {DebugMode::None, "None"},
                                 {DebugMode::AO, "AO"},
                                 {DebugMode::GIProbe, "GIProbe"},
                                 {DebugMode::Normal, "Normal"},
                                 {DebugMode::AABB, "AABB"},
                                 {DebugMode::Bloom, "Bloom"},
                                 {DebugMode::Velocity, "Velocity"},
                                 {DebugMode::GI, "GI"},
                                 {DebugMode::ShadowMask, "ShadowMask"},
                                 {DebugMode::Albedo, "Albedo"},
                                 {DebugMode::Emission, "Emission"},
                                 {DebugMode::Metallic, "Metallic"},
                                 {DebugMode::Roughness, "Roughness"},
                                 {DebugMode::ShadowMap, "ShadowMap"},
                                 {DebugMode::Specular, "Specular"},
                                 {DebugMode::WorldTangent, "WorldTangent"},
                                 {DebugMode::SceneDepth, "SceneDepth"},
                                 {DebugMode::Opacity, "Opacity"},
                                 {DebugMode::PreTonemapHDR, "PreTonemapHDR"},
                                 {DebugMode::PostTonemapHDR, "PostTonemapHDR"},
                                 {DebugMode::LightingOnly, "LightingOnly"},
                                 {DebugMode::ShadingModel, "ShadingModel"}
                                 })

    struct BufferVisualizationEntry
    {
        DebugMode mode;
        const char* label;
    };

    // UE [Engine.BufferVisualizationMaterials] names, in the order shown in the
    // viewport View Mode menu. Only buffers this renderer actually stores.
    inline constexpr BufferVisualizationEntry kBufferVisualizationModes[] =
    {
        {DebugMode::Albedo, "Base Color"},
        {DebugMode::ShadingModel, "Shading Model"},
        {DebugMode::Metallic, "Metallic"},
        {DebugMode::Roughness, "Roughness"},
        {DebugMode::Normal, "World Normal"},
        {DebugMode::WorldTangent, "World Tangent"},
        {DebugMode::Specular, "Specular"},
        {DebugMode::SceneDepth, "Scene Depth"},
        {DebugMode::AO, "AO"},
        {DebugMode::Velocity, "Velocity"},
        {DebugMode::PreTonemapHDR, "Pre Tonemap HDR Color"},
        {DebugMode::PostTonemapHDR, "Post Tonemap HDR Color"},
        {DebugMode::Opacity, "Opacity"},
    };

    inline constexpr BufferVisualizationEntry kShadowVisualizationModes[] =
    {
        {DebugMode::ShadowMask, "Shadow Mask"},
        {DebugMode::ShadowMap, "Shadow Map"},
    };

    inline constexpr BufferVisualizationEntry kGIVisualizationModes[] =
    {
        {DebugMode::GI, "GI Scene"},
        {DebugMode::GIProbe, "GI Probe"},
        {DebugMode::AABB, "AABB"},
    };

    // Extra overlays that stay in Render Settings, not the viewport View Mode menu.
    inline constexpr DebugMode kDebugOverlayModes[] =
    {
        DebugMode::Bloom,
    };

    inline bool ModeListContains(const BufferVisualizationEntry* entries, size_t count, DebugMode mode)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (entries[i].mode == mode)
                return true;
        }
        return false;
    }

    inline const char* ModeListLabel(const BufferVisualizationEntry* entries, size_t count, DebugMode mode)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (entries[i].mode == mode)
                return entries[i].label;
        }
        return nullptr;
    }

    inline bool IsBufferVisualizationMode(DebugMode mode)
    {
        return ModeListContains(kBufferVisualizationModes,
                                sizeof(kBufferVisualizationModes) / sizeof(kBufferVisualizationModes[0]),
                                mode);
    }

    inline bool IsShadowVisualizationMode(DebugMode mode)
    {
        return ModeListContains(kShadowVisualizationModes,
                                sizeof(kShadowVisualizationModes) / sizeof(kShadowVisualizationModes[0]),
                                mode);
    }

    inline bool IsGIVisualizationMode(DebugMode mode)
    {
        return ModeListContains(kGIVisualizationModes,
                                sizeof(kGIVisualizationModes) / sizeof(kGIVisualizationModes[0]),
                                mode);
    }

    inline bool IsViewportViewMode(DebugMode mode)
    {
        return mode == DebugMode::None ||
               mode == DebugMode::Emission ||
               mode == DebugMode::LightingOnly ||
               IsBufferVisualizationMode(mode) ||
               IsShadowVisualizationMode(mode) ||
               IsGIVisualizationMode(mode);
    }

    inline bool IsDebugOverlayMode(DebugMode mode)
    {
        for (DebugMode overlay : kDebugOverlayModes)
        {
            if (overlay == mode)
                return true;
        }
        return false;
    }

    inline const char* GetViewportViewModeLabel(DebugMode mode)
    {
        if (mode == DebugMode::None)
            return "Lit";
        if (mode == DebugMode::Emission)
            return "Unlit";
        if (mode == DebugMode::LightingOnly)
            return "Lighting Only";
        if (const char* label = ModeListLabel(kBufferVisualizationModes,
                                              sizeof(kBufferVisualizationModes) / sizeof(kBufferVisualizationModes[0]),
                                              mode))
            return label;
        if (const char* label = ModeListLabel(kShadowVisualizationModes,
                                              sizeof(kShadowVisualizationModes) / sizeof(kShadowVisualizationModes[0]),
                                              mode))
            return label;
        if (const char* label = ModeListLabel(kGIVisualizationModes,
                                              sizeof(kGIVisualizationModes) / sizeof(kGIVisualizationModes[0]),
                                              mode))
            return label;
        return "Lit";
    }
}