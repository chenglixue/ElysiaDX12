#include "stdafx.h"
#include "../public/SelectionManager.h"

#include "Runtime/Engine/ECS/public/Entity.h"
#include "Programs/public/Log.h"

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;

    void SelectionManager::Select(Entity* pEntity)
    {
        m_pSelected = pEntity;
    }

    void SelectionManager::Clear()
    {
        m_pSelected = nullptr;
    }

    void SelectionManager::ResolvePick(Entity* pEntity)
    {
        Select(pEntity);
        m_bScrollToSelected = (pEntity != nullptr);

        ElysiaHelper::Log::Info(
            "[Selection] hit-proxy resolved: %s",
            pEntity ? pEntity->name.c_str() : "<background>");
    }

    bool SelectionManager::IsScrollTargetAncestor(const Entity* pEntity) const
    {
        if (!m_bScrollToSelected || m_pSelected == nullptr || pEntity == nullptr)
        {
            return false;
        }

        // Walk up from the selected entity: pEntity is an ancestor when the walk
        // passes through it.
        for (const Entity* pAncestor = m_pSelected->GetParent();
             pAncestor != nullptr;
             pAncestor = pAncestor->GetParent())
        {
            if (pAncestor == pEntity)
            {
                return true;
            }
        }
        return false;
    }
}
