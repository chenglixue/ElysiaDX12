#pragma once
#include "Transform.h"
#include "Runtime/RenderCore/DX12Camera.h"

namespace ElysiaRenderer
{
    class MeshRenderer;
}

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;

    struct Entity
    {
    public:
        eastl::string name = "";
        Transform transform;
        std::unique_ptr<MeshRenderer> pMeshRenderer = nullptr;
        DX12Camera* pAttachedCamera = nullptr;

        ~Entity();
        Entity(eastl::string n);
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;

        void Init(Transform transform)
        {
            this->transform = transform;

            UpdateWorldAABB();
        }

        void SetName(eastl::string name)
        {
            this->name = name;
        }
        void SetParent(Entity* pParent);
        void SetLocalAABB(Vector3 aabbMin, Vector3 aabbMax) noexcept
        {
            Vector3 center = (aabbMin + aabbMax) * 0.5f;
            Vector3 extents = (aabbMax - aabbMin) * 0.5f;

            m_localAABB.Center = center;
            m_localAABB.Extents = extents;
        }

        const std::vector<std::unique_ptr<Entity>>& GetChildren()
        {
            return m_childs;
        };
        Entity* GetParent() const noexcept
        {
            return m_pParent;
        }
        BoundingBox GetLocalAABB() const noexcept
        {
            return m_localAABB;
        }
        BoundingBox GetWorldAABB() const noexcept
        {
            return m_worldAABB;
        }

        void AddChild(std::unique_ptr<Entity>&& child);

        bool IsDirty() const
        {
            return m_IsDirty;
        }
        void ClearDirty()
        {
            m_IsDirty = false;
        }
        void OnTransformChanged();
        void UpdateWorldAABB();

    private:
        bool m_IsDirty = true;
        Entity* m_pParent = nullptr;
        std::vector<std::unique_ptr<Entity>> m_childs;
        BoundingBox m_worldAABB;
        BoundingBox m_localAABB;

        BoundingBox LocalAABB() const noexcept;
        bool HasMeshRenderer() const noexcept;
    };
}