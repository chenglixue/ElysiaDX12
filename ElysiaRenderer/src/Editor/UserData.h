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
#include "../Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/SubsurfaceScatterUtility.h"

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
        //L"glTF\\Sponza\\Sponza.gltf",
        L"glTF\\Suzanne\\Suzanne.gltf",
        //L"glTF\\SM_MatPreviewMesh_01\\SM_MatPreviewMesh_01.gltf",
        // L"glTF\\head\\head.gltf",
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

        ShadingModel shadingModelID = ShadingModel::DefaultLit;
        Vector3 BaseColorTint = Vector3::One;
        float Opacity = 1;
        float Cutoff = 0.5;
        float NormalIntensity = 1;
        float MetallicIntensity = 1;
        float RoughnessIntensity = 1;
        float AmbientCubemapIntensity = 1;
        float Specular = 1;
        Vector3 AmbientCubemapTint = Vector3::One;
        Vector3 EmissionTint = Vector3::One;

        SubsurfaceScatterParameter subsurfaceScatterParameter{};
        ShadowParameter shadowParameter;

        HDRParameter hdrParameter;

        AOParameter aoParameter{};
        GIParameter GIParameter{};

        DebugMode debugMode = DebugMode::None;
        int mipmapLevel = 0;
        int instanceID = 0;

        BloomParameter bloomParameter{};
        TAAParameter taaParameter{};
        SharpenParameter sharpenParameter{};

        bool EnableHIZ = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserData,
                                                    lightColor,
                                                    lightDir,
                                                    lightIntensity,
                                                    shadingModelID,
                                                    BaseColorTint,
                                                    Opacity,
                                                    Cutoff,
                                                    NormalIntensity,
                                                    MetallicIntensity,
                                                    RoughnessIntensity,
                                                    AmbientCubemapIntensity,
                                                    Specular,
                                                    AmbientCubemapTint,
                                                    EmissionTint,
                                                    subsurfaceScatterParameter,
                                                    shadowParameter,
                                                    hdrParameter,
                                                    aoParameter,
                                                    GIParameter,
                                                    debugMode,
                                                    mipmapLevel,
                                                    instanceID,
                                                    bloomParameter,
                                                    taaParameter,
                                                    sharpenParameter,
                                                    EnableHIZ)

    private:
        static std::unique_ptr<UserData> m_instance;
        static std::once_flag m_initInstanceFlag;
    };


    void DeSerializeUserData();

    void SerializeUserData();
}