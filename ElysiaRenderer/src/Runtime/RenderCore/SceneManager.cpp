#include "stdafx.h"
#include "SceneManager.h"

#include "CameraManager.h"
#include "DX12Camera.h"
#include "MeshRenderer.h"
#include "Editor/UserData.h"
#include "Runtime/Core/DX12UploadContext.h"
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
        // UpdateEntities();
    }
    void SceneManager::Destory()
    {

    }

    void SceneManager::LoadScene(UINT& loadStage)
    {
        WCHAR assetsPath[512];
        GetAssetsPath(assetsPath, _countof(assetsPath));

        std::vector<std::shared_ptr<LoadedModel>> loadedModels;
        for (const auto& modelPath : g_ModelPaths)
        {
            loadedModels.emplace_back(
                std::move(CreateModel(ElysiaHelper::GetAssetFullPath(assetsPath, modelPath))));
        }

        if (loadStage == 6)
        {
            for (auto& loadedModel : loadedModels)
            {
                CreateEntityFromModel(loadedModel);
            }
        }
        if (loadStage == 7)
        {
            CollectRenderItems();
        }

        if (loadStage > 7)
        {
            if (!m_pDevice->GetUploadContext()->HasWork())
            {
                loadStage = 0;
                return;
            }
        }

        loadStage ++;
    }

    std::shared_ptr<ElysiaModel::LoadedModel> SceneManager::CreateModel(
        const std::wstring& modelPath)
    {
        return ModelManager::GetInstance().LoadStaticModel(modelPath, 1.f);
    }
    Entity* SceneManager::CreateEntityFromModel(std::shared_ptr<ElysiaModel::LoadedModel> pModel)
    {
        auto pEntity = CreateEntity(pModel);

        Entity* ptr = pEntity.get();

        m_entities.emplace_back(std::move(pEntity));
        return ptr;
    }
    std::unique_ptr<Entity> SceneManager::CreateEntity(
        const std::shared_ptr<LoadedModel>& model) const
    {
        auto pParent = std::make_unique<Entity>(ToEastl(model->name));
        pParent->transform.scale = Vector3::One * 0.0001f;
        pParent->transform.rotation = MathHelper::Euler(90, 0, 180);
        pParent->SetLocalAABB(model->aabbMin, model->aabbMax);
        pParent->UpdateWorldAABB();

        UINT meshIndex = 0;
        for (auto mesh : model->meshes)
        {
            auto pChild = std::make_unique<Entity>(ToEastl(mesh.name));
            pChild->transform = Transform
            {
                .position = Vector3::Zero,
                .rotation = Quaternion::Identity,
                .scale = Vector3::One
            };
            pChild->transform.m_pParent = &pParent->transform;
            pChild->pMeshRenderer = std::make_unique<MeshRenderer>();
            pChild->pMeshRenderer->ShutDown();
            pChild->pMeshRenderer->Init(model, meshIndex);
            pChild->SetLocalAABB(mesh.aabbMin, mesh.aabbMax);
            pChild->UpdateWorldAABB();

            pParent->AddChild(std::move(pChild));
            meshIndex ++;
        }

        return pParent;
    }

    void SceneManager::CollectRenderItems()
    {
        renderList.clear();

        auto viewFrustum = CameraManager::GetInstance().GetMainCamera()->GetFrustum();
        for (const auto& pEntity : m_entities)
        {
            CollectRenderItem(pEntity, viewFrustum);
        }
        SortRenderItems();
    }
    void SceneManager::CollectRenderItem(const std::unique_ptr<Entity>& pEntity,
                                         BoundingFrustum& cameraFrustum)
    {
        switch (cameraFrustum.Contains(pEntity->GetWorldAABB()))
        {
        case CONTAINS:
            std::cout << "CONTAINS" << std::endl;
            break;
        case DISJOINT:
            std::cout << "DISJOINT" << std::endl;
            break;
        case INTERSECTS:
            std::cout << "INTERSECTS" << std::endl;
            break;
        }
        // if (cameraFrustum.Contains(pEntity->GetWorldAABB()) != DISJOINT)
        {
            if (pEntity->pMeshRenderer != nullptr)
            {
                const auto& worldMat = pEntity->transform.GetWorldMatrix();
                const auto& mesh = pEntity->pMeshRenderer->GetMesh();

                RenderItem item
                {
                    .vbView = pEntity->pMeshRenderer->GetVertexBufferView(),
                    .ibView = pEntity->pMeshRenderer->GetIndexBufferView(),
                    .indexCount = mesh.numIndices,
                    .startIndex = mesh.idxOffset,
                    .baseVertex = mesh.vtxOffset,
                    .pAssociatedEntity = pEntity.get(),
                    .worldMatrix = worldMat,
                    .textureIndices = pEntity->pMeshRenderer->GetTextureIndices(),
                    .loadedMaterial = pEntity->pMeshRenderer->GetMaterial(),
                    .distanceToCameraSq = Vector3::DistanceSquared(
                        pEntity->transform.GetPosition(),
                        CameraManager::GetInstance().GetMainCamera()->GetPosition())
                };
                renderList.emplace_back(std::move(item));
            }
            for (const auto& childEntity : pEntity->GetChildren())
            {
                CollectRenderItem(childEntity, cameraFrustum);
            }
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
        if (pEntity == nullptr)
            return;

        const auto& worldMat = pEntity->GetParent()
                                   ? pEntity->transform.GetWorldMatrix() * pEntity->GetParent()->
                                                                                    transform.
                                                                                    GetWorldMatrix()
                                   : pEntity->transform.GetWorldMatrix();
        if (pEntity->pMeshRenderer != nullptr)
        {
            auto& mesh = pEntity->pMeshRenderer->GetMesh();
            mesh.aabbMin = Vector3(FLT_MAX);
            mesh.aabbMax = Vector3(-FLT_MAX);
            for (UINT32 vertexIndex = 0; vertexIndex < mesh.numVertices; vertexIndex ++)
            {
                Vector3 position = Vector3::Transform(
                    pEntity->pMeshRenderer->GetVertices()[vertexIndex].Position,
                    worldMat);

                mesh.aabbMin.x = eastl::min(mesh.aabbMin.x, position.x);
                mesh.aabbMin.y = eastl::min(mesh.aabbMin.y, position.y);
                mesh.aabbMin.z = eastl::min(mesh.aabbMin.z, position.z);

                mesh.aabbMax.x = eastl::max(mesh.aabbMax.x, position.x);
                mesh.aabbMax.y = eastl::max(mesh.aabbMax.y, position.y);
                mesh.aabbMax.z = eastl::max(mesh.aabbMax.z, position.z);
            }
        }

        for (auto& pChild : pEntity->GetChildren())
        {
            UpdateEntity(pChild);
        }
    }

    void SceneManager::SortRenderItems()
    {
        std::sort(renderList.begin(),
                  renderList.end(),
                  [](const RenderItem& a, const RenderItem& b)
                  {
                      // 按深度从前到后排序（Early-Z）
                      return a.distanceToCameraSq < b.distanceToCameraSq;
                  });
    }
}