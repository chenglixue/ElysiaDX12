#pragma once

#include "Transform.h"
#include "Runtime/RenderCore/public/ShadowUtility.h"
#include <algorithm>
#include <cmath>

namespace ElysiaEngine
{
    using ElysiaRenderer::ShadowParameter;

    // Scene-authored light, analogous to UE's UDirectionalLightComponent on an
    // ADirectionalLight actor (ULightComponentBase + ULightComponent +
    // UDirectionalLightComponent). Shading still has a single "main" sun (the
    // first light with bAtmosphereSun, else the first directional), matching UE's
    // AtmosphereSunLightIndex 0 path. Extra directional lights are scene objects
    // and persist; they do not yet accumulate in the deferred lighting loop.
    //
    // GPU lighting/CSM still reads UserData (color/dir/intensity + shadowParameter).
    // LightManager mirrors the main sun onto that runtime state each frame.
    struct LightComponent
    {
        enum class Type : uint8_t
        {
            Directional = 0
        };

        Type type = Type::Directional;

        // ULightComponentBase::LightColor / Intensity (Category = Light).
        Vector3 color = Vector3::One;
        float intensity = 1.f;
        // UDirectionalLightComponent::LightSourceAngle (Category = Light, degrees).
        float sourceAngleDegrees = 0.5357f;
        // UDirectionalLightComponent::bAtmosphereSunLight (Category = AtmosphereAndCloud).
        bool bAtmosphereSun = false;

        // ULightComponentBase::CastShadows plus ULightComponent AdvancedDisplay
        // ShadowBias / ShadowSlopeBias, UDirectionalLightComponent CascadedShadowMaps
        // DynamicShadowDistance, and this renderer's quality / PCF extras.
        ShadowParameter shadow{};
    };

    // UE ULightComponent::GetDirection: light rays travel along local +X.
    inline Vector3 GetDirectionalLightDirection(const Transform& transform)
    {
        const Matrix world = transform.GetWorldMatrix();
        Vector3 dir(world._11, world._12, world._13);
        if (dir.LengthSquared() < 1.0e-12f)
            return Vector3(1.0e-5f, 0.f, 0.f);
        dir.Normalize();
        return dir;
    }

    inline Quaternion RotationFromLightDirection(Vector3 direction)
    {
        if (direction.LengthSquared() < 1.0e-12f)
            direction = Vector3(1.0e-5f, 0.f, 0.f);
        else
            direction.Normalize();

        const Vector3 from = Vector3::UnitX;
        const float cosAngle = from.Dot(direction);
        if (cosAngle > 0.9999f)
            return Quaternion::Identity;
        if (cosAngle < -0.9999f)
            return Quaternion::CreateFromAxisAngle(Vector3::UnitY, XM_PI);

        Vector3 axis = from.Cross(direction);
        axis.Normalize();
        const float clamped = (std::max)(-1.f, (std::min)(1.f, cosAngle));
        return Quaternion::CreateFromAxisAngle(axis, acosf(clamped));
    }
}
