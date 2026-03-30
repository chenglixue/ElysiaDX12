#pragma once
#include "Runtime/Resource/Serialization.h"
#include "Runtime/RenderCore/ShadowUtility.h"
#include "Runtime/RenderCore/TonemapUtility.h"
#include "ThirdParty/ColorConversion.h"
#include "Runtime/RenderCore/AOUtility.h"
#include "Programs/DebugUtility.h"
#include "Runtime/RenderCore/BloomUtility.h"
#include "Runtime/RenderCore/CASUtility.h"
#include "Runtime/RenderCore/GIUtility.h"
#include "Runtime/RenderCore/TAAUtility.h"
#include "ThirdParty/FreesyncHDR.h"

namespace DirectX
{
    namespace SimpleMath
    {
        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Vector3, x, y, z)
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(ColorSpace,
                             {
                             {ColorSpace::ColorSpace_REC709,
                             "ColorSpace_REC709"},
                             {ColorSpace::ColorSpace_P3,
                             "ColorSpace_P3"},
                             {ColorSpace::ColorSpace_REC2020,
                             "ColorSpace_REC2020"},
                             {ColorSpace::ColorSpace_Display,
                             "ColorSpace_Display"}
                             })

namespace CAULDRON_DX12
{
    NLOHMANN_JSON_SERIALIZE_ENUM(DisplayMode,
                                 {
                                 {DisplayMode::DISPLAYMODE_SDR,
                                 "DISPLAYMODE_SDR"},
                                 {DisplayMode::DISPLAYMODE_FSHDR_Gamma22,
                                 "DISPLAYMODE_FSHDR_Gamma22"},
                                 {DisplayMode::DISPLAYMODE_FSHDR_SCRGB,
                                 "DISPLAYMODE_FSHDR_SCRGB"},
                                 {DisplayMode::DISPLAYMODE_HDR10_2084,
                                 "DISPLAYMODE_HDR10_2084"},
                                 {DisplayMode::DISPLAYMODE_HDR10_SCRGB,
                                 "DISPLAYMODE_HDR10_SCRGB"}
                                 })
}


namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    const std::vector<LPCWSTR> g_ModelPaths
    {
        L"glTF\\Sponza\\Sponza.gltf",
        // L"glTF\\DragonAttenuation\\DragonAttenuation.gltf",
    };

    class UserData
    {
    public:
        UserData()
        {
        }
        UserData(const UserData&) = delete;
        UserData& operator=(const UserData&) = delete;
        UserData(UserData&&) = delete;
        UserData& operator=(UserData&&) = delete;

        static UserData& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new UserData());
                           });

            return *m_instance;
        }

        Vector3 lightColor = Vector3::One;
        Vector3 lightDir = Vector3::One;
        float lightIntensity = 1.f;

        Vector3 BaseColorTint = Vector3::One;
        float Opacity = 1;
        float Cutoff = 0.5;
        float NormalIntensity = 1;
        float MetallicIntensity = 1;
        float RoughnessIntensity = 1;
        float AmbientCubemapIntensity = 1;
        Vector3 AmbientCubemapTint = Vector3::One;

        ShadowType shadowType = ShadowType::Soft;
        ShadowQuality shadowQuality = ShadowQuality::VeryHigh;
        float shadowDepthBias = 0;
        float shadowSlopeDepthBias = 0;
        float shadowMaxSlopeDepthBias = 0;

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

        AOParameter aoParameter{};
        GIParameter GIParameter{};

        DebugMode debugMode = DebugMode::None;
        int mipmapLevel = 0;
        int instanceID = 0;

        bool EnableShadow = true;

        BloomParameter bloomParameter{};
        TAAParameter taaParameter{};
        SharpenParameter sharpenParameter{};

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserData,
                                                    lightColor,
                                                    lightDir,
                                                    lightIntensity,
                                                    BaseColorTint,
                                                    Opacity,
                                                    Cutoff,
                                                    NormalIntensity,
                                                    MetallicIntensity,
                                                    RoughnessIntensity,
                                                    AmbientCubemapIntensity,
                                                    AmbientCubemapTint,
                                                    shadowType,
                                                    shadowQuality,
                                                    shadowDepthBias,
                                                    shadowSlopeDepthBias,
                                                    shadowMaxSlopeDepthBias,
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
                                                    Crosstalk,
                                                    aoParameter,
                                                    GIParameter,
                                                    debugMode,
                                                    mipmapLevel,
                                                    instanceID,
                                                    EnableShadow,
                                                    bloomParameter,
                                                    taaParameter,
                                                    sharpenParameter)

    private:
        static std::unique_ptr<UserData> m_instance;
        static std::once_flag m_initInstanceFlag;
    };


    void DeSerializeUserData();

    void SerializeUserData();
}