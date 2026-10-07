#pragma once
#include "Programs/public/IManager.h"
#include "RenderItem.h"
#include "Runtime/Engine/ECS/public/LightComponent.h"

namespace ElysiaModel
{
    struct LoadedModel;
}

namespace ElysiaEngine
{
    struct Entity;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    enum class BasicShapeType : uint8_t;

    class SceneManager : IManager, IUpdate
    {
    public:
        SceneManager();
        ~SceneManager();
        std::vector<RenderItem> renderList;

        static SceneManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new SceneManager());
                           });

            return *m_instance;
        }

        virtual void Init(ElysiaCore::DX12Device* pDevice) override;
        virtual void Destory() override;
        virtual void Update(const FrameContext& context) override;

        void LoadScene(UINT&);

        std::shared_ptr<ElysiaModel::LoadedModel> CreateModel(const std::wstring& modelPath);
        Entity* CreateEntityFromModel(std::shared_ptr<ElysiaModel::LoadedModel> pModel,
                                      const Vector3& position,
                                      const Quaternion& rotation = Quaternion::Identity,
                                      const Vector3& scale = Vector3::One);
        Entity* SpawnDirectionalLight();
        Entity* SpawnBasicShape(BasicShapeType type);
        Entity* FindMainDirectionalLight() const;
        void SetAtmosphereSun(Entity* pLightEntity);
        void DestroyRootEntity(Entity* pEntity);
        void CollectRenderItems();
        void ClearScene();

        // UE EWorldType::Editor vs EWorldType::PIE. Play snapshots the live
        // scene (CreatePIEWorldByDuplication analog) and restores it on Stop.
        bool IsPlaying() const;
        void BeginPlay();
        void EndPlay();

        void UpdateEntities();
        std::vector<std::unique_ptr<Entity>>& GetEntities()
        {
            return m_entities;
        }
        std::unique_ptr<Entity>& GetRootEntity()
        {
            return m_entities[0];
        }
        void AddEntity(std::unique_ptr<Entity> pEntity)
        {
            m_entities.emplace_back(std::move(pEntity));
        }

        void SortRenderItems();

    private:
        SceneManager(const SceneManager& rhs) = delete;
        SceneManager& operator=(SceneManager& rhs) = delete;
        SceneManager(SceneManager&& rhs) = default;

        std::unique_ptr<Entity> CreateEntity(
            const std::shared_ptr<ElysiaModel::LoadedModel>& model,
            const Vector3& position,
            const Quaternion& rotation,
            const Vector3& scale) const;
        void UpdateEntity(const std::unique_ptr<Entity>& pEntity);
        void CollectRenderItem(const std::unique_ptr<Entity>& pEntity,
                               BoundingFrustum& boundingFrustum);
        void SpawnSavedDirectionalLights();
        void SpawnSavedBasicShapes();
        void CapturePlaySnapshot(Entity& entity);

        enum class WorldMode : uint8_t
        {
            Editor = 0,
            Play = 1
        };
        struct PlayEntitySnapshot
        {
            Entity* pEntity = nullptr;
            Transform transform{};
            LightComponent light{};
            bool bHasLight = false;
        };

        ElysiaCore::DX12Device* m_pDevice = nullptr;
        WorldMode m_worldMode = WorldMode::Editor;
        std::vector<PlayEntitySnapshot> m_playSnapshots;
        static std::unique_ptr<SceneManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        std::mutex m_mutex;
        std::vector<std::unique_ptr<Entity>> m_entities;
        std::vector<std::shared_ptr<ElysiaModel::LoadedModel>> m_pendingModels;
    };
}