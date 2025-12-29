#include "stdafx.h"
#include "Entity.h"

#include "Runtime/RenderCore/MeshRenderer.h"

namespace ElysiaEngine
{
    Entity::Entity(eastl::string n) :
        name(std::move(n))
    {
        
    }

    Entity::~Entity()
    {
        
    }
    void Entity::AddChild(std::unique_ptr<Entity>&& pChild)
    {
        pChild->SetParent(this);
        m_childs.emplace_back(std::move(pChild));
    }

    void Entity::SetParent(Entity* pParent)
    {
        m_pParent = pParent;
    }
}