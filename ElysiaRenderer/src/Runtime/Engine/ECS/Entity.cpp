#include "stdafx.h"
#include "Entity.h"

#include "Runtime/RenderCore/MeshRenderer.h"

namespace ElysiaEngine
{
    Entity::Entity(eastl::string n)
        : name(std::move(n))
    {

    }

    Entity::~Entity()
    {

    }

    BoundingBox Entity::LocalAABB() const noexcept
    {
        return pMeshRenderer->GetBoundingBox();
    }
    bool Entity::HasMeshRenderer() const noexcept
    {
        return pMeshRenderer != nullptr;
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

    void Entity::OnTransformChanged()
    {
        m_IsDirty = true;
        if (pAttachedCamera)
        {
            pAttachedCamera->m_transform = transform;

            auto fpCam = dynamic_cast<FirstPersonCamera*>(pAttachedCamera);
            if (fpCam)
            {
                fpCam->SyncFromTransform();
            }
            pAttachedCamera->UpdateViewMatrix();
            pAttachedCamera->UpdateFrustum();
        }
    }

    void Entity::UpdateWorldAABB()
    {
        auto world_M = transform.GetWorldMatrix();
        m_localAABB.Transform(m_worldAABB, world_M);
    }
}