#include "stdafx.h"
#include "Entity.h"

namespace ElysiaEngine
{
    void Entity::AddChild(std::unique_ptr<Entity> pChild)
    {
        pChild->SetParent(this);
        m_childs.emplace_back(std::move(pChild));
    }

    void Entity::SetParent(Entity* pParent)
    {
        m_pParent = pParent;
    }
}