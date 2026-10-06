#include "stdafx.h"
#include "../public/LightManager.h"

#include "Editor/public/UserData.h"

#include "Runtime/Engine/public/ElysiaFrame.h"

#include "Runtime/RenderCore/Pass/public/ShadowPass.h"
#include "../public/DX12Light.h"
#include "../public/DX12Shadow.h"
#include "Runtime/RenderCore/public/SceneManager.h"

namespace ElysiaRenderer
{
    std::unique_ptr<LightManager> LightManager::m_instance;
    std::once_flag LightManager::m_initInstanceFlag;

    LightManager::LightManager() = default;
    LightManager::~LightManager()
    {
        Destory();
    }

    void LightManager::Init(ElysiaCore::DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
        CreatMainLight();

    }

    void LightManager::Destory()
    {

    }

    void LightManager::Update(const ElysiaEngine::FrameContext& context)
    {
        m_frameID = context.frameID;
        m_frameIndex = context.frameIndex;

        auto& pUsetData = UserData::GetInstance();

        m_pMainLight->m_lightColor = pUsetData.lightColor;
        m_pMainLight->m_lightDir = pUsetData.lightDir;
        m_pMainLight->m_lightIntensity = pUsetData.lightIntensity;

        std::vector<BoundingBox> casterBounds;
        auto& entities = SceneManager::GetInstance().GetEntities();
        casterBounds.reserve(entities.size());
        for (const auto& entity : entities)
        {
            if (!entity)
                AppendCasterBounds(*entity, casterBounds);
            const BoundingBox box = entity->GetWorldAABB();
            if (box.Extents.x <= 0.f && box.Extents.y <= 0.f && box.Extents.z <= 0.f)
                continue;
            casterBounds.push_back(box);
        }

        if (context.pCamera != nullptr)
        {
            m_pMainLight->GetMainShadow()->UpdateShadowTransform(
                m_pMainLight.get(),
                *context.pCamera,
                pUsetData.shadowParameter.shadowDistance,
                casterBounds.data(),
                casterBounds.size());
        }
    }

    DX12DirectionLight* LightManager::GetMainLight()
    {
        return m_pMainLight.get();
    }
    DX12Shadow* LightManager::GetMainShadow()
    {
        return m_pMainLight->GetMainShadow();
    }
    RenderTexture* LightManager::GetMainShadowRT() const
    {
        return m_pMainLight->GetMainShadowRT();
    }

    void LightManager::CreatMainLight()
    {
        auto& pUserData = UserData::GetInstance();
        if (m_pMainLight != nullptr)
        {
            m_pMainLight.reset();
            m_pMainLight = std::make_unique<DX12DirectionLight>(
                pUserData.lightColor,
                pUserData.lightDir,
                pUserData.lightIntensity);
        }
        else
        {
            m_pMainLight = std::make_unique<DX12DirectionLight>(
                pUserData.lightColor,
                pUserData.lightDir,
                pUserData.lightIntensity);
        }
    }

    void LightManager::AppendCasterBounds(Entity& entity, std::vector<BoundingBox>& casterBounds)
    {
        if (entity.pMeshRenderer != nullptr)
        {
            BoundingBox worldBounds;
            entity.GetLocalAABB().Transform(worldBounds, entity.transform.GetWorldMatrix());
            if (worldBounds.Extents.x > 0.f || worldBounds.Extents.y > 0.f || worldBounds.Extents.z > 0.f)
                casterBounds.push_back(worldBounds);
        }

        for (auto& child : entity.GetChildren())
        {
            if (child)
                AppendCasterBounds(*child, casterBounds);
        }
    }
}