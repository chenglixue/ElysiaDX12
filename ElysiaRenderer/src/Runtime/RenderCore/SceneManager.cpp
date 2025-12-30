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
    void SceneManager::Update(const FrameContext& context)
    {
        UpdateEntities();
    }
    void SceneManager::Destory()
    {
        
    }

    void SceneManager::LoadScene(std::vector<RenderItem>& outRenderList)
    {
        if (!_CrtCheckMemory()) {
            __debugbreak(); 
        }
        
        WCHAR assetsPath[512];
        GetAssetsPath(assetsPath, _countof(assetsPath));

        for (const auto& modelPath : g_ModelPaths)
        {
            CreateEntityFromModel(ElysiaHelper::GetAssetFullPath(assetsPath, modelPath).c_str());
        }
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
    std::unique_ptr<Entity> SceneManager::CreateEntity(const std::shared_ptr<LoadedModel>& model) const
    {
        auto pParent = std::make_unique<Entity>(ToEastl(model->name));
        pParent->transform.scale = Vector3::One * model->scale;
        pParent->transform.position = (model->aabbMin + model->aabbMax) * 0.5f;

        UINT meshIndex = 0;
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
            pChild->pMeshRenderer->Init(model, meshIndex);
            
            pParent->AddChild(std::move(pChild));
            meshIndex++;
        }

        return pParent;
    }

    void SceneManager::CollectRenderItems(std::vector<RenderItem>& outList) const
    {
        outList.clear();
        
        for(UINT64 entityIndex = 0; entityIndex < m_entities.size(); entityIndex++)
        {
            const auto& pEntity = m_entities[entityIndex];
            CollectRenderItem(pEntity, outList);
        }
    }
    void SceneManager::CollectRenderItem(const std::unique_ptr<Entity>& pEntity, std::vector<RenderItem>& outList) const
    {
        if (pEntity->pMeshRenderer != nullptr)
        {
            const auto& worldMat = pEntity->GetParent() ?
                pEntity->transform.GetWorldMatrix() * pEntity->GetParent()->transform.GetWorldMatrix() :
                pEntity->transform.GetWorldMatrix();

            const auto& mesh = pEntity->pMeshRenderer->GetMesh();
            RenderItem item
            {
                .vbView = pEntity->pMeshRenderer->GetVertexBufferView(),
                .ibView = pEntity->pMeshRenderer->GetIndexBufferView(),
                .indexCount = mesh.numIndices,
                .startIndex = mesh.idxOffset,
                .baseVertex = mesh.vtxOffset,
                .worldMatrix = worldMat,
                .textureIndices = pEntity->pMeshRenderer->GetTextureIndices(),
                .loadedMaterial = pEntity->pMeshRenderer->GetMaterial()
            };
            outList.emplace_back(std::move(item));
        }
        for (const auto& childEntity : pEntity->GetChildren())
        {
            CollectRenderItem(childEntity, outList);
        }
    }
    
    void SceneManager::ClearScene()
    {
        m_entities.clear();
    }

    void SceneManager::UpdateEntities()
    {
        for (auto& entity : m_entities)
        {
            UpdateEntity(entity);
        }
    }
    void SceneManager::UpdateEntity(const std::unique_ptr<Entity>& pEntity)
    {
        if (pEntity == nullptr) return;
        
        const auto& worldMat = pEntity->GetParent() ? pEntity->transform.GetWorldMatrix() * pEntity->GetParent()->transform.GetWorldMatrix() :
            pEntity->transform.GetWorldMatrix();
        if (pEntity->pMeshRenderer != nullptr)
        {
            auto& mesh = pEntity->pMeshRenderer->GetMesh();
            for (UINT32 vertexIndex = 0; vertexIndex < mesh.numVertices; vertexIndex++)
            {
                mesh.aabbMin = Vector3(FLT_MAX);
                mesh.aabbMax = Vector3(-FLT_MAX);
                Vector3 position = Vector3::Transform(pEntity->pMeshRenderer->GetVertices()[vertexIndex].Position, worldMat);

                mesh.aabbMin.x = eastl::min(mesh.aabbMin.x, position.x);
                mesh.aabbMin.y = eastl::min(mesh.aabbMin.y, position.y);
                mesh.aabbMin.z = eastl::min(mesh.aabbMin.z, position.z);
                
                mesh.aabbMax.x = eastl::max(mesh.aabbMax.x, position.x);
                mesh.aabbMax.y = eastl::max(mesh.aabbMax.y, position.y);
                mesh.aabbMax.z = eastl::max(mesh.aabbMax.z, position.z);
            }

            pEntity->pMeshRenderer->Update();
        }

        for (auto& pChild : pEntity->GetChildren())
        {
            UpdateEntity(pChild);
        }
    }

}