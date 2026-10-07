#include "stdafx.h"
#include "../public/SceneManager.h"

#include "../public/CameraManager.h"
#include "../public/DX12Camera.h"
#include "../public/MeshRenderer.h"
#include "../public/SelectionManager.h"
#include "Editor/public/UserData.h"
#include "Runtime/Core/public/DX12UploadContext.h"
#include "Runtime/Resource/Model/public/LoadedModel.h"
#include "Runtime/Resource/Model/public/ModelManager.h"
#include "Programs/public/Helper.h"
#include "Programs/public/Log.h"
#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/Engine/ECS/public/LightComponent.h"

#include <algorithm>

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

        if (loadStage == 0)
        {
            m_pendingModels.clear();
            for (const auto& modelPath : g_ModelPaths)
            {
                m_pendingModels.emplace_back(
                    std::move(CreateModel(ElysiaHelper::GetAssetFullPath(assetsPath, modelPath.c_str()))));
            }
        }

        if (loadStage == 6)
        {
            // The first model stays at the origin. Each later model is placed
            // just past the previous model's right edge so they do not overlap.
            // Saved transforms from Engine.ini win over this default packing.
            constexpr float kModelGap = 1.f;
            float nextOriginX = 0.f;
            bool isFirstModel = true;
            for (size_t modelIndex = 0; modelIndex < m_pendingModels.size(); ++modelIndex)
            {
                auto& loadedModel = m_pendingModels[modelIndex];
                Vector3 position = Vector3::Zero;
                Quaternion rotation = Quaternion::Identity;
                Vector3 scale = Vector3::One;
                const bool hasBounds = loadedModel && loadedModel->aabbMin.x <= loadedModel->aabbMax.x;
                const bool hasSaved = modelIndex < g_ModelTransforms.size() &&
                                      g_ModelTransforms[modelIndex].valid;
                if (hasSaved)
                {
                    position = g_ModelTransforms[modelIndex].location;
                    Transform savedRotation;
                    savedRotation.SetEulerDegrees(g_ModelTransforms[modelIndex].rotationEuler);
                    rotation = savedRotation.rotation;
                    scale = g_ModelTransforms[modelIndex].scale;
                }
                else if (!isFirstModel && hasBounds)
                {
                    position.x = nextOriginX - loadedModel->aabbMin.x;
                }

                if (Entity* pEntity = CreateEntityFromModel(loadedModel, position, rotation, scale))
                    pEntity->sourceModelIndex = static_cast<int>(modelIndex);

                if (hasBounds)
                    nextOriginX = position.x + loadedModel->aabbMax.x + kModelGap;
                isFirstModel = false;
            }

            SpawnSavedDirectionalLights();
            SpawnSavedBasicShapes();
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
        auto pModel = ModelManager::GetInstance().LoadStaticModel(modelPath, 1.f);

        return pModel;
    }
    Entity* SceneManager::CreateEntityFromModel(std::shared_ptr<ElysiaModel::LoadedModel> pModel,
                                                 const Vector3& position,
                                                 const Quaternion& rotation,
                                                 const Vector3& scale)
    {
        if (!pModel || pModel->meshes.empty())
        {
            ElysiaHelper::Log::Warn("SceneManager: skip \"%s\" because it has no meshes.",
                                    pModel ? pModel->name.c_str() : "(null)");
            return nullptr;
        }

        auto pEntity = CreateEntity(pModel, position, rotation, scale);

        Entity* ptr = pEntity.get();

        m_entities.emplace_back(std::move(pEntity));
        return ptr;
    }
    std::unique_ptr<Entity> SceneManager::CreateEntity(
        const std::shared_ptr<LoadedModel>& model,
        const Vector3& position,
        const Quaternion& rotation,
        const Vector3& scale) const
    {
        auto pParent = std::make_unique<Entity>(ToEastl(model->name));
        pParent->transform.position = position;
        pParent->transform.rotation = rotation;
        pParent->transform.scale = scale;
        // pParent->transform.rotation = MathHelper::Euler(-45, 0, 0);
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

    namespace
    {
        constexpr float kLightActorExtent = 0.25f;

        std::string MakeUniqueActorName(const std::string& baseName,
                                        const std::vector<std::unique_ptr<Entity>>& entities)
        {
            for (int n = 1;; ++n)
            {
                const std::string name = (n == 1)
                                             ? baseName
                                             : (baseName + " " + std::to_string(n));
                bool used = false;
                for (const auto& entity : entities)
                {
                    if (entity && entity->name == name.c_str())
                    {
                        used = true;
                        break;
                    }
                }
                if (!used)
                    return name;
            }
        }

        std::string MakeUniqueDirectionalLightName(const std::vector<std::unique_ptr<Entity>>& entities)
        {
            return MakeUniqueActorName("Directional Light", entities);
        }

        Entity* CreateShapeEntity(const SavedBasicShape& saved)
        {
            auto pModel = ModelManager::GetInstance().GetOrCreateBasicShape(
                static_cast<uint8_t>(saved.type));
            if (!pModel)
                return nullptr;

            Transform rotation;
            rotation.SetEulerDegrees(saved.rotationEuler);
            Entity* pEntity = SceneManager::GetInstance().CreateEntityFromModel(
                pModel,
                saved.location,
                rotation.rotation,
                saved.scale);
            if (!pEntity)
                return nullptr;

            pEntity->name = ToEastl(saved.name);
            pEntity->sourceShapeType = static_cast<uint8_t>(saved.type);
            return pEntity;
        }

        Entity* CreateLightEntity(const SavedDirectionalLight& saved)
        {
            auto pEntity = std::make_unique<Entity>(ToEastl(saved.name));
            pEntity->transform.position = saved.location;
            pEntity->transform.SetEulerDegrees(saved.rotationEuler);
            pEntity->transform.scale = Vector3::One;
            pEntity->pLight = std::make_unique<LightComponent>();
            pEntity->pLight->type = LightComponent::Type::Directional;
            pEntity->pLight->color = saved.color;
            pEntity->pLight->intensity = saved.intensity;
            pEntity->pLight->sourceAngleDegrees = saved.sourceAngleDegrees;
            pEntity->pLight->bAtmosphereSun = saved.atmosphereSun;
            pEntity->pLight->shadow = saved.shadow;
            pEntity->SetLocalAABB(Vector3(-kLightActorExtent, -kLightActorExtent, -kLightActorExtent),
                                  Vector3(kLightActorExtent, kLightActorExtent, kLightActorExtent));
            pEntity->UpdateWorldAABB();
            Entity* ptr = pEntity.get();
            SceneManager::GetInstance().AddEntity(std::move(pEntity));
            return ptr;
        }
    }

    void SceneManager::SpawnSavedDirectionalLights()
    {
        if (g_HasExplicitDirectionalLights)
        {
            for (size_t i = 0; i < g_DirectionalLights.size(); ++i)
            {
                Entity* pEntity = CreateLightEntity(g_DirectionalLights[i]);
                pEntity->sourceLightIndex = static_cast<int>(i);
            }
        }
        else
        {
            const auto& data = UserData::GetInstance();
            SavedDirectionalLight migrated;
            migrated.name = "Directional Light";
            migrated.location = Vector3::Zero;
            Transform rotation;
            rotation.rotation = RotationFromLightDirection(data.lightDir);
            migrated.rotationEuler = rotation.GetEulerDegrees();
            migrated.color = data.lightColor;
            migrated.intensity = data.lightIntensity;
            migrated.sourceAngleDegrees = data.lightSourceAngleDegrees;
            migrated.atmosphereSun = true;
            migrated.shadow = data.shadowParameter;
            Entity* pEntity = CreateLightEntity(migrated);
            pEntity->sourceLightIndex = 0;
            g_HasExplicitDirectionalLights = true;
        }

        g_DirectionalLightsSpawned = true;
    }

    Entity* SceneManager::SpawnDirectionalLight()
    {
        if (IsPlaying())
            return nullptr;

        const auto& data = UserData::GetInstance();
        SavedDirectionalLight desc;
        desc.name = MakeUniqueDirectionalLightName(m_entities);
        desc.color = data.lightColor;
        desc.intensity = data.lightIntensity;
        desc.sourceAngleDegrees = data.lightSourceAngleDegrees;
        desc.shadow = data.shadowParameter;
        desc.atmosphereSun = FindMainDirectionalLight() == nullptr;

        if (DX12Camera* pCamera = CameraManager::GetInstance().GetMainCamera())
            desc.location = pCamera->GetPosition() + pCamera->GetForwardDir() * 3.f;

        if (Entity* pMain = FindMainDirectionalLight())
        {
            Transform rotation;
            rotation.rotation = RotationFromLightDirection(GetDirectionalLightDirection(pMain->transform));
            desc.rotationEuler = rotation.GetEulerDegrees();
            desc.color = pMain->pLight->color;
            desc.intensity = pMain->pLight->intensity;
            desc.sourceAngleDegrees = pMain->pLight->sourceAngleDegrees;
            desc.shadow = pMain->pLight->shadow;
        }
        else
        {
            Transform rotation;
            rotation.rotation = RotationFromLightDirection(data.lightDir);
            desc.rotationEuler = rotation.GetEulerDegrees();
        }

        Entity* pEntity = CreateLightEntity(desc);
        pEntity->sourceLightIndex = static_cast<int>(g_DirectionalLights.size());
        g_HasExplicitDirectionalLights = true;
        g_DirectionalLightsSpawned = true;
        return pEntity;
    }

    void SceneManager::SpawnSavedBasicShapes()
    {
        if (g_HasExplicitBasicShapes)
        {
            for (size_t i = 0; i < g_BasicShapes.size(); ++i)
            {
                Entity* pEntity = CreateShapeEntity(g_BasicShapes[i]);
                if (!pEntity)
                    continue;
                pEntity->sourceShapeIndex = static_cast<int>(i);
            }
        }

        g_BasicShapesSpawned = true;
    }

    Entity* SceneManager::SpawnBasicShape(BasicShapeType type)
    {
        if (IsPlaying() || !IsValidBasicShapeType(type))
            return nullptr;

        SavedBasicShape desc;
        desc.type = type;
        desc.name = MakeUniqueActorName(GetBasicShapeTypeName(type), m_entities);
        desc.scale = Vector3::One;

        if (DX12Camera* pCamera = CameraManager::GetInstance().GetMainCamera())
            desc.location = pCamera->GetPosition() + pCamera->GetForwardDir() * 3.f;

        Entity* pEntity = CreateShapeEntity(desc);
        if (!pEntity)
            return nullptr;

        pEntity->sourceShapeIndex = static_cast<int>(g_BasicShapes.size());
        g_HasExplicitBasicShapes = true;
        g_BasicShapesSpawned = true;
        CollectRenderItems();
        return pEntity;
    }

    Entity* SceneManager::FindMainDirectionalLight() const
    {
        Entity* pFirst = nullptr;
        for (const auto& entity : m_entities)
        {
            if (!entity || !entity->pLight)
                continue;
            if (entity->pLight->type != LightComponent::Type::Directional)
                continue;
            if (!pFirst)
                pFirst = entity.get();
            if (entity->pLight->bAtmosphereSun)
                return entity.get();
        }
        return pFirst;
    }

    void SceneManager::SetAtmosphereSun(Entity* pLightEntity)
    {
        if (!pLightEntity || !pLightEntity->pLight || IsPlaying())
            return;

        for (auto& entity : m_entities)
        {
            if (!entity || !entity->pLight)
                continue;
            entity->pLight->bAtmosphereSun = (entity.get() == pLightEntity);
        }
    }

    void SceneManager::DestroyRootEntity(Entity* pEntity)
    {
        if (!pEntity || IsPlaying())
            return;

        if (SelectionManager::GetInstance().GetSelected() == pEntity)
            SelectionManager::GetInstance().Clear();

        auto it = std::find_if(m_entities.begin(),
                               m_entities.end(),
                               [pEntity](const std::unique_ptr<Entity>& candidate)
                               {
                                   return candidate.get() == pEntity;
                               });
        if (it != m_entities.end())
            m_entities.erase(it);

        if (CameraManager::GetInstance().GetMainCamera())
            CollectRenderItems();
    }

    bool SceneManager::IsPlaying() const
    {
        return m_worldMode == WorldMode::Play;
    }

    void SceneManager::CapturePlaySnapshot(Entity& entity)
    {
        PlayEntitySnapshot snapshot;
        snapshot.pEntity = &entity;
        snapshot.transform = entity.transform;
        if (entity.pLight)
        {
            snapshot.light = *entity.pLight;
            snapshot.bHasLight = true;
        }
        m_playSnapshots.push_back(snapshot);
        for (auto& child : entity.GetChildren())
        {
            if (child)
                CapturePlaySnapshot(*child);
        }
    }

    void SceneManager::BeginPlay()
    {
        if (m_worldMode == WorldMode::Play)
            return;

        m_playSnapshots.clear();
        for (auto& entity : m_entities)
        {
            if (entity)
                CapturePlaySnapshot(*entity);
        }
        m_worldMode = WorldMode::Play;
    }

    void SceneManager::EndPlay()
    {
        if (m_worldMode != WorldMode::Play)
            return;

        for (const auto& snapshot : m_playSnapshots)
        {
            if (!snapshot.pEntity)
                continue;
            snapshot.pEntity->transform = snapshot.transform;
            if (snapshot.bHasLight && snapshot.pEntity->pLight)
                *snapshot.pEntity->pLight = snapshot.light;
            snapshot.pEntity->OnTransformChanged();
        }
        m_playSnapshots.clear();
        m_worldMode = WorldMode::Editor;
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
        // The selection (and any pending hit-proxy readback) holds raw Entity
        // pointers; drop them before the entities are destroyed.
        SelectionManager::GetInstance().Clear();
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