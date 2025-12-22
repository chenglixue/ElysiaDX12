#include "stdafx.h"
#include "SceneManager.h"

#include "ModelManager.h"

namespace ElysiaRenderer
{
    std::unique_ptr<SceneManager> m_instance;
    std::once_flag m_initInstanceFlag;

    SceneManager::~SceneManager()
    {
        Destory();
    }

    void SceneManager::Init(DX12Device* pDevice)
    {
        m_pDevice = pDevice;
    }
    void SceneManager::Destory()
    {
        
    }

    Entity* SceneManager::CreateEntity(const eastl::string& name, const eastl::wstring& modelPath)
    {
        auto pEntity = std::make_unique<Entity>(name);
        pEntity->pMeshRenderer = std::make_unique<MeshRenderer>();
        auto model = ModelManager::GetInstance().LoadStaticModel(ToStdWString(modelPath).c_str(), 1);
        pEntity->pMeshRenderer->ShutDown();
        pEntity->pMeshRenderer->Init(model);
        
        Entity* ptr = pEntity.get();
        m_entities.emplace_back(std::move(pEntity));
        return ptr;
    }
    void SceneManager::CollectRenderItems(std::vector<RenderItem>& outList)
    {
        outList.clear();
        
        for(const auto& pEntity : m_entities)
        {
            if(!pEntity->pMeshRenderer) continue;
            
            const auto* pModel = pEntity->pMeshRenderer->GetModel();
            const auto& worldMat = pEntity->transform->GetWorldMatrix();
            
            for(UINT64 meshIdx = 0; meshIdx < pModel->meshes.size(); meshIdx++)
            {
                const auto& mesh = pModel->meshes[meshIdx];
                
                RenderItem item
                {
                    .vbView = mesh.vbView,
                    .ibView = mesh.ibView,
                    .indexCount = mesh.numIndices,
                    .startIndex = mesh.idxOffset,
                    .baseVertex = (INT)mesh.vtxOffset
                };
                
                item.worldMatrix = worldMat;
                item.textureIndices = pEntity->pMeshRenderer->GetTextureIndices(meshIdx);
                outList.emplace_back(std::move(item));
            }
        }
    }
    void SceneManager::ClearScene()
    {
        m_entities.clear();
    }
}