#pragma once
#include <string>
#include <vector>

#include "Runtime/Resource/public/Serialization.h"
#include "Runtime/RenderCore/public/ShadowUtility.h"
#include "Runtime/RenderCore/public/TonemapUtility.h"
#include "ThirdParty/ColorConversion.h"
#include "Runtime/RenderCore/public/AOUtility.h"
#include "Programs/public/DebugUtility.h"
#include "Runtime/RenderCore/public/BloomUtility.h"
#include "Runtime/RenderCore/public/CASUtility.h"
#include "Runtime/RenderCore/public/GIUtility.h"
#include "Runtime/RenderCore/public/TAAUtility.h"
#include "ThirdParty/FreesyncHDR.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/HairUtility.h"
#include "Runtime/RenderCore/public/SSSRUtility.h"
#include "Runtime/RenderCore/public/SubsurfaceScatterUtility.h"

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

    // Filled from [Startup] +ModelPath in the ini hierarchy.
    extern std::vector<std::wstring> g_ModelPaths;

    // Parallel to g_ModelPaths. Restored from [Startup] ModelLocation / ModelRotation /
    // ModelScale, then kept in sync with live root-entity transforms.
    struct SavedModelTransform
    {
        Vector3 location = Vector3::Zero;
        Vector3 rotationEuler = Vector3::Zero; // pitch, yaw, roll in degrees
        Vector3 scale = Vector3::One;
        bool valid = false;
    };
    extern std::vector<SavedModelTransform> g_ModelTransforms;

    // Parallel to scene directional-light entities. Restored from [Startup]
    // DirectionalLight* arrays. Missing arrays mean "migrate the legacy [Light]
    // singleton into one actor" on first load.
    struct SavedDirectionalLight
    {
        std::string name = "Directional Light";
        Vector3 location = Vector3::Zero;
        Vector3 rotationEuler = Vector3::Zero; // pitch, yaw, roll in degrees
        Vector3 color = Vector3::One;
        float intensity = 1.f;
        float sourceAngleDegrees = 0.5357f;
        bool atmosphereSun = true;
        // Per-actor shadow, same fields as ULightComponent / CascadedShadowMaps.
        // Missing ini arrays fall back to the global [Shadow] spawn template.
        ShadowParameter shadow{};
    };
    extern std::vector<SavedDirectionalLight> g_DirectionalLights;
    // True when [Startup] DirectionalLightLocation was present (including an
    // explicit empty list). False means spawn the legacy [Light] singleton.
    extern bool g_HasExplicitDirectionalLights;
    // True after LoadScene has spawned light actors (or decided there are none).
    // SerializeUserData only rewrites g_DirectionalLights once this is set, so a
    // mid-load save cannot wipe the restored list.
    extern bool g_DirectionalLightsSpawned;

    // UE Place Actors / Shapes: Cube, Sphere, Plane (Cylinder/Cone are not placed).
    enum class BasicShapeType : uint8_t
    {
        Cube = 0,
        Sphere = 1,
        Plane = 2
    };

    inline const char* GetBasicShapeTypeName(BasicShapeType type)
    {
        switch (type)
        {
        case BasicShapeType::Sphere:
            return "Sphere";
        case BasicShapeType::Plane:
            return "Plane";
        default:
            return "Cube";
        }
    }

    inline bool IsValidBasicShapeType(BasicShapeType type)
    {
        return type == BasicShapeType::Cube ||
               type == BasicShapeType::Sphere ||
               type == BasicShapeType::Plane;
    }

    // Parallel to placed basic-shape root entities. Restored from [Startup]
    // BasicShape* arrays. Missing arrays mean "no shapes" on first load and
    // must not write empty arrays until the user actually places one.
    struct SavedBasicShape
    {
        std::string name = "Cube";
        BasicShapeType type = BasicShapeType::Cube;
        Vector3 location = Vector3::Zero;
        Vector3 rotationEuler = Vector3::Zero; // pitch, yaw, roll in degrees
        Vector3 scale = Vector3::One;
    };
    extern std::vector<SavedBasicShape> g_BasicShapes;
    extern bool g_HasExplicitBasicShapes;
    extern bool g_BasicShapesSpawned;

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
        // Directional light "source angle": angular diameter of the sun disc in
        // degrees (0.5357 = the real sun), matching UE's directional light SourceAngle.
        // A directional light sits at infinity, so it has an angle rather than a
        // world-space radius (that one belongs to point/spot lights).
        float lightSourceAngleDegrees = 0.5357f;

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
        HairParameter hairParameter{};
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
        SSSRParameter sssrParameter{};

        bool EnableHIZ = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserData,
                                                    lightColor,
                                                    lightDir,
                                                    lightIntensity,
                                                    lightSourceAngleDegrees,
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
                                                    hairParameter,
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
                                                    sssrParameter,
                                                    EnableHIZ)

    private:
        static std::unique_ptr<UserData> m_instance;
        static std::once_flag m_initInstanceFlag;
    };


    void DeSerializeUserData();

    void SerializeUserData();
}