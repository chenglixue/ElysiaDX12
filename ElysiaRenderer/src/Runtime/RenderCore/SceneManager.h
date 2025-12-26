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
    
    class SceneManager : IManager
    {
    public:
        ~SceneManager();
        
        static SceneManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag, []()
            {
                m_instance.reset(new SceneManager());
            });

            return *m_instance;
        }
		
        virtual void Init(ElysiaCore::DX12Device* pDevice) override;
        virtual void Destory() override;

        void LoadScene(std::vector<RenderItem>& outRenderList);
        
        Entity* CreateEntityFromModel(const eastl::wstring& modelPath);
        void CollectRenderItems(std::vector<RenderItem>& outList) const;
        void ClearScene();
        
    private:
        SceneManager() = default;
        SceneManager(const SceneManager& rhs) = delete;
        SceneManager& operator=(SceneManager& rhs) = delete;
        SceneManager(SceneManager&& rhs) = default;
        std::unique_ptr<Entity> CreateEntity(const std::shared_ptr<ElysiaModel::LoadedModel>& model) const;
        
        ElysiaCore::DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<SceneManager> m_instance;
        static std::once_flag m_initInstanceFlag;
		
        std::mutex m_mutex;
        std::vector<std::unique_ptr<Entity>> m_entities;
    };
}

