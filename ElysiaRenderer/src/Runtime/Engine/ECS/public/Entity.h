#pragma once
#include "Transform.h"
#include "LightComponent.h"
#include "Runtime/Core/public/BufferUtility.h"
#include "Runtime/RenderCore/public/DX12Camera.h"

namespace ElysiaCore
{
    class DX12GraphicsContext;
}

namespace ElysiaRenderer
{
    class MeshRenderer;
}

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;
    using namespace ElysiaCore;

    struct Entity
    {
    public:
        eastl::string name = "";
        Transform transform;
        std::unique_ptr<MeshRenderer> pMeshRenderer = nullptr;
        std::unique_ptr<LightComponent> pLight = nullptr;
        DX12Camera* pAttachedCamera = nullptr;
        // Index into g_ModelPaths / g_ModelTransforms for the root entity of a
        // loaded model. -1 for mesh children and anything not spawned from Startup.
        int sourceModelIndex = -1;
        // Index into g_DirectionalLights for a spawned directional light. -1 otherwise.
        int sourceLightIndex = -1;
        // Index into g_BasicShapes for a placed Cube/Sphere/Plane root. -1 otherwise.
        int sourceShapeIndex = -1;
        // BasicShapeType. Valid when sourceShapeIndex >= 0.
        uint8_t sourceShapeType = 0;

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
        BufferHandle GetBLASBuffer() const noexcept
        {
            return m_pBLASBuffer;
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
        void GenerateBLAS(ID3D12Device5* pDevice, ElysiaCore::DX12GraphicsContext* pCommand);

    private:
        bool m_IsDirty = true;
        Entity* m_pParent = nullptr;
        std::vector<std::unique_ptr<Entity>> m_childs;
        BoundingBox m_worldAABB;
        BoundingBox m_localAABB;
        BufferHandle m_pBLASBuffer;
        BufferHandle m_pBLASScratchBuffer;
        BufferHandle m_pBLASDescBuffer;

        BoundingBox LocalAABB() const noexcept;
        bool HasMeshRenderer() const noexcept;
    };
}