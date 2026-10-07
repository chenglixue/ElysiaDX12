#include "stdafx.h"

#include "../public/UserData.h"
#include "../public/ConfigCache.h"

#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/Engine/ECS/public/LightComponent.h"
#include "Runtime/RenderCore/public/SceneManager.h"

namespace ElysiaRenderer
{
    std::once_flag UserData::m_initInstanceFlag;
    std::unique_ptr<UserData> UserData::m_instance;
    std::vector<std::wstring> g_ModelPaths;
    std::vector<SavedModelTransform> g_ModelTransforms;
    std::vector<SavedDirectionalLight> g_DirectionalLights;
    bool g_HasExplicitDirectionalLights = false;
    bool g_DirectionalLightsSpawned = false;
    std::vector<SavedBasicShape> g_BasicShapes;
    bool g_HasExplicitBasicShapes = false;
    bool g_BasicShapesSpawned = false;

    void DeSerializeUserData()
    {
        ConfigCache::Get().LoadHierarchy();
        ConfigCache::Get().Apply();
    }

    void SerializeUserData()
    {
        // Scene load happens across several frames. Until root entities exist,
        // keep whatever Apply() restored so a mid-load save cannot wipe them.
        const auto& entities = SceneManager::GetInstance().GetEntities();
        // UE does not write the PIE world back to the editor map. Keep the last
        // editor snapshot in g_* while playing so Stop cannot dirty Saved ini.
        if (!SceneManager::GetInstance().IsPlaying() && !entities.empty())
        {
            if (g_ModelTransforms.size() < g_ModelPaths.size())
                g_ModelTransforms.resize(g_ModelPaths.size());

            for (const auto& entity : entities)
            {
                if (!entity || entity->sourceModelIndex < 0)
                    continue;

                const size_t i = static_cast<size_t>(entity->sourceModelIndex);
                if (i >= g_ModelTransforms.size())
                    g_ModelTransforms.resize(i + 1);

                auto& saved = g_ModelTransforms[i];
                saved.location = entity->transform.position;
                saved.rotationEuler = entity->transform.GetEulerDegrees();
                saved.scale = entity->transform.scale;
                saved.valid = true;
            }

            if (g_DirectionalLightsSpawned)
            {
                g_DirectionalLights.clear();
                g_HasExplicitDirectionalLights = true;
                for (const auto& entity : entities)
                {
                    if (!entity || !entity->pLight)
                        continue;

                    SavedDirectionalLight savedLight;
                    savedLight.name = entity->name.c_str();
                    savedLight.location = entity->transform.position;
                    savedLight.rotationEuler = entity->transform.GetEulerDegrees();
                    savedLight.color = entity->pLight->color;
                    savedLight.intensity = entity->pLight->intensity;
                    savedLight.sourceAngleDegrees = entity->pLight->sourceAngleDegrees;
                    savedLight.atmosphereSun = entity->pLight->bAtmosphereSun;
                    savedLight.shadow = entity->pLight->shadow;
                    entity->sourceLightIndex = static_cast<int>(g_DirectionalLights.size());
                    g_DirectionalLights.push_back(std::move(savedLight));
                }
            }

            if (g_BasicShapesSpawned)
            {
                std::vector<SavedBasicShape> rebuilt;
                for (const auto& entity : entities)
                {
                    if (!entity || entity->sourceShapeIndex < 0)
                        continue;

                    SavedBasicShape savedShape;
                    savedShape.name = entity->name.c_str();
                    savedShape.type = static_cast<BasicShapeType>(entity->sourceShapeType);
                    if (!IsValidBasicShapeType(savedShape.type))
                        savedShape.type = BasicShapeType::Cube;
                    savedShape.location = entity->transform.position;
                    savedShape.rotationEuler = entity->transform.GetEulerDegrees();
                    savedShape.scale = entity->transform.scale;
                    entity->sourceShapeIndex = static_cast<int>(rebuilt.size());
                    rebuilt.push_back(std::move(savedShape));
                }
                if (g_HasExplicitBasicShapes || !rebuilt.empty())
                {
                    g_HasExplicitBasicShapes = true;
                    g_BasicShapes = std::move(rebuilt);
                }
            }
        }

        ConfigCache::Get().SaveDiff();
    }
}
