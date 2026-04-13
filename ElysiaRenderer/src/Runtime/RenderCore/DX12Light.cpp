#include "stdafx.h"
#include "DX12Light.h"

#include "Programs/Helper.h"

#include "Editor/UserData.h"

#include "Runtime/RenderCore/RenderTexture.h"
#include "RenderResource.h"
#include "DX12Shadow.h"
#include "RenderTargetManager.h"
#include "Pass/ShadowPass.h"

namespace ElysiaRenderer
{

    DX12Light::DX12Light(Vector3 lightColor, Vector3 lightDir, float intensity)
        : m_lightType(LightType::None),
          m_lightColor(lightColor),
          m_lightDir(lightDir),
          m_lightIntensity(intensity)
    {
    }

    DX12DirectionLight::DX12DirectionLight(Vector3 lightColor, Vector3 lightDir, float intensity)
        : DX12Light(lightColor, lightDir, intensity),
          m_lightType(LightType::Dir)
    {
    }

    LightType DX12Light::GetLightType() const
    {
        return m_lightType;
    }
    Vector3 DX12Light::GetLightColor() const
    {
        return m_lightColor;
    }
    Vector3 DX12Light::GetLightDir() const
    {
        if (XMVector3Equal(m_lightDir, XMVectorZero()))
        {
            return {1e-5, 0, 0};
        }
        return m_lightDir;
    }
    float DX12Light::GetLightIntensity() const
    {
        return m_lightIntensity;
    }

    void DX12Light::SetLightColor(const Vector3& lightColor)
    {
        m_lightColor = lightColor;
    }
    void DX12Light::SetLightDir(const Vector3& lightDir)
    {
        m_lightDir = lightDir;
    }
    void DX12Light::SetLightIntensity(float lightIntensity)
    {
        m_lightIntensity = lightIntensity;
    }

    DX12DirectionLight::DX12DirectionLight() = default;
    DX12DirectionLight::~DX12DirectionLight() = default;

    LightData DX12DirectionLight::CreateLightData()
    {
        LightData o{};

        o.m_lightColor = Vector4(m_lightColor.x, m_lightColor.y, m_lightColor.z, 1.f);
        o.m_lightDir = Vector4(m_lightDir.x, m_lightDir.y, m_lightDir.z, 0.f);
        o.m_intensity = m_lightIntensity;
        o.m_lightPos = Vector4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
        o.m_falloffStart = FLT_MAX;
        o.m_falloffEnd = FLT_MAX;
        o.m_spotPower = FLT_MAX;

        return o;
    }
    void DX12DirectionLight::CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format)
    {
        float resolution;
        switch (UserData::GetInstance().shadowParameter.shadowQuality)
        {
        case ShadowQuality::Low:
        {
            resolution = 512;
            break;
        }
        case ShadowQuality::Middle:
        {
            resolution = 1024;
            break;
        }
        case ShadowQuality::High:
        {
            resolution = 2048;
            break;
        }
        case ShadowQuality::VeryHigh:
        {
            resolution = 4096 * 2;
            break;
        }
        default:
        {
            resolution = 1024;
            break;
        }
        }
        m_pShadowRT = RenderTargetManager::GetInstance().CreateRenderTexture(
            static_cast<UINT64>(resolution),
            static_cast<UINT64>(resolution),
            format,
            true,
            RenderResource::GetInstance().GetPropertyName(ShadowPass::RenderTextureIDs::ShadowRTID));

        auto shadowMap = std::make_unique<DX12Shadow>(m_pShadowRT->GetTexture());
        shadowMap->InitBoundSphere(boundSphereRadius);

        if (m_pMainShadow != nullptr)
        {
            m_pMainShadow.reset();
            m_pMainShadow = std::move(shadowMap);
        }
        else
        {
            m_pMainShadow = std::move(shadowMap);
        }
    }
    RenderTexture* DX12DirectionLight::GetMainShadowRT() const noexcept
    {
        assert(m_pMainShadow != nullptr);
        return m_pShadowRT;
    }

    DX12Shadow* DX12DirectionLight::GetMainShadow() const noexcept
    {
        return m_pMainShadow.get();
    }

}