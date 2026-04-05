#pragma once
#include "Programs/IManager.h"
#include "src/Runtime/RenderCore/RenderItem.h"

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
        Entity* CreateEntityFromModel(std::shared_ptr<ElysiaModel::LoadedModel> pModel);
        void CollectRenderItems();
        void ClearScene();

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
            const std::shared_ptr<ElysiaModel::LoadedModel>& model) const;
        void UpdateEntity(const std::unique_ptr<Entity>& pEntity);
        void CollectRenderItem(const std::unique_ptr<Entity>& pEntity,
                               BoundingFrustum& boundingFrustum);

        ElysiaCore::DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<SceneManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        std::mutex m_mutex;
        std::vector<std::unique_ptr<Entity>> m_entities;
    };
}