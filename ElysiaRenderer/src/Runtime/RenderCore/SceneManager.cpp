#include "stdafx.h"
#include "SceneManager.h"

#include "MeshRenderer.h"
#include "Editor/UserData.h"
#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Resource/Model/ModelManager.h"
#include "Runtime/Engine/ECS/Entity.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;
    
    std::unique_ptr<SceneManager> SceneManager::m_instance;
    std::once_flag SceneManager::m_initInstanceFlag;

    SceneManager::SceneManager()
    {
        
    }
    
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

    void SceneManager::LoadScene(std::vector<RenderItem>& outRenderList)
    {
        CreateEntityFromModel(g_ModelPaths[0]);
        CollectRenderItems(outRenderList);
    }

    Entity* SceneManager::CreateEntityFromModel(const eastl::wstring& modelPath)
    {
        auto model = ModelManager::GetInstance().LoadStaticModel(ToStdWString(modelPath).c_str(), 1);
        
        auto pEntity = CreateEntity(model);
        
        Entity* ptr = pEntity.get();
        m_entities.emplace_back(std::move(pEntity));
        return ptr;
    }
    std::unique_ptr<Entity> SceneManager::CreateEntity(const std::shared_ptr<ElysiaModel::LoadedModel>& model) const
    {
        auto pParent = std::make_unique<Entity>(ToEastl(model->name));
        pParent->transform.scale = Vector3(model->scale);
        pParent->transform.position = (model->aabbMin + model->aabbMax) * 0.5f;

        for (auto mesh : model->meshes)
        {
            auto pChild = std::make_unique<Entity>(ToEastl(mesh.name));
            pChild->transform =
            {
                .position = Vector3::Zero,
                .rotation = Quaternion::Identity,
                .scale = Vector3::One,
            };
            pChild->pMeshRenderer = std::make_unique<MeshRenderer>();
            pChild->pMeshRenderer->ShutDown();
            pChild->pMeshRenderer->Init(model);
            
            pParent->AddChild(std::move(pChild));
        }

        return pParent;
    }

    void SceneManager::CollectRenderItems(std::vector<RenderItem>& outList) const
    {
        outList.clear();
        
        for(const auto& pEntity : m_entities)
        {
            if(!pEntity->pMeshRenderer) continue;
            
            const auto* pModel = pEntity->pMeshRenderer->GetModel();
            const auto& worldMat = pEntity->transform.GetWorldMatrix();
            
            for(UINT64 meshIdx = 0; meshIdx < pModel->meshes.size(); meshIdx++)
            {
                const auto& mesh = pModel->meshes[meshIdx];
                
                RenderItem item
                {
                    .vbView = mesh.vbView,
                    .ibView = mesh.ibView,
                    .indexCount = mesh.numIndices,
                    .startIndex = mesh.idxOffset,
                    .baseVertex = INT(mesh.vtxOffset)
                };
                
                item.worldMatrix = worldMat;
                item.loadedMaterial = pModel->materials[mesh.materialIndex];
                outList.emplace_back(std::move(item));
            }
        }
    }
    void SceneManager::ClearScene()
    {
        m_entities.clear();
    }
}