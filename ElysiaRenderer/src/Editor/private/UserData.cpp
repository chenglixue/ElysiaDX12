#include "stdafx.h"

#include "../public/UserData.h"
#include "../public/ConfigCache.h"

#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/RenderCore/public/SceneManager.h"

namespace ElysiaRenderer
{
    std::once_flag UserData::m_initInstanceFlag;
    std::unique_ptr<UserData> UserData::m_instance;
    std::vector<std::wstring> g_ModelPaths;
    std::vector<SavedModelTransform> g_ModelTransforms;

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
        if (!entities.empty())
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
        }

        ConfigCache::Get().SaveDiff();
    }
}
