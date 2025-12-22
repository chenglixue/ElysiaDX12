#pragma once
#include "IManager.h"
#include "ECS/Entity.h"
#include "src/Runtime/RenderCore/RenderItem.h"

namespace ElysiaRenderer
{
    class SceneManager : ElysiaRenderer::IManager
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
		
        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;
        
        Entity* CreateEntity(const eastl::string& name, const eastl::wstring& modelPath);
        void CollectRenderItems(std::vector<RenderItem>& outList);
        void ClearScene();
        
    private:
        SceneManager() = default;
        SceneManager(const SceneManager& rhs) = delete;
        SceneManager& operator=(SceneManager& rhs) = delete;
        SceneManager(SceneManager&& rhs) = default;
        
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<SceneManager> m_instance;
        static std::once_flag m_initInstanceFlag;
		
        std::mutex m_mutex;
        std::vector<std::unique_ptr<Entity>> m_entities;
    };
}

