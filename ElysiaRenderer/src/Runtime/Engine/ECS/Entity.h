#pragma once
#include "Transform.h"

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
        eastl::string name;
        Transform transform;
        std::unique_ptr<MeshRenderer> pMeshRenderer = nullptr;

        ~Entity();
        Entity(eastl::string n);
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;

        void AddChild(std::unique_ptr<Entity>&& child);
        void SetParent(Entity* pParent);
        const std::vector<std::unique_ptr<Entity>>& GetChildren()
        {
            return m_childs;
        };
        Entity* GetParent() const noexcept
        {
            return m_pParent;
        }

        void SetName(eastl::string name)
        {
            this->name = name;
        }

        bool IsDirty() const
        {
            return m_IsDirty;
        }
        void ClearDirty()
        {
            m_IsDirty = false;
        }
        void OnTransformChanged()
        {
            m_IsDirty = true;
        }

    private:
        bool m_IsDirty = true;
        Entity* m_pParent = nullptr;
        std::vector<std::unique_ptr<Entity>> m_childs;
    };
}