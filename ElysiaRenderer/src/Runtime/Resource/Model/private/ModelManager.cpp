#include "stdafx.h"
#include "../public/ModelManager.h"

#include "../public/AssimpLoader.h"
#include "../public/AssimpLoader.h"
#include "../public/LoadedModel.h"
#include "../public/ModelCache.h"
#include "../public/MaterialOverrides.h"
#include "Editor/public/UserData.h"
#include "Programs/public/Hash.h"
#include "Programs/public/Log.h"

#include <cmath>

namespace ElysiaRenderer
{
    std::unique_ptr<ModelManager> ModelManager::m_instance;
    std::once_flag ModelManager::m_initInstanceFlag;

    static size_t MakeModelCacheKey(const std::wstring& filePath,
                                    bool bInvertTexcoordY,
                                    bool bImportMeshes,
                                    bool bImportSkeletons,
                                    bool bImportAnimations,
                                    float scale)
    {
        UINT32 scaleBits = 0;
        memcpy(&scaleBits, &scale, sizeof(scaleBits));

        UINT32 flags = 0;
        if (bInvertTexcoordY)
            flags |= 1u << 0;
        if (bImportMeshes)
            flags |= 1u << 1;
        if (bImportSkeletons)
            flags |= 1u << 2;
        if (bImportAnimations)
            flags |= 1u << 3;

        const UINT32 extra[2] = {scaleBits, flags};
        size_t key = xxh::GetHash(filePath);
        const size_t extraHash = xxh::GetHash(extra, sizeof(extra));
        key ^= extraHash + 0x9e3779b9 + (key << 6) + (key >> 2);
        return key;
    }

    ModelManager::~ModelManager()
    {

    }

    void ModelManager::Init(DX12Device* pDevice)
    {
        m_pDevice = pDevice;
    }

    void ModelManager::FlushMaterialEdits()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& entry : m_modelCache)
        {
            if (auto model = entry.second.lock())
                ElysiaModel::MaterialOverrides::SaveIfDirty(*model);
        }
    }

    void ModelManager::Destory()
    {
        FlushMaterialEdits();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_modelCache.clear();
        m_basicShapes[0].reset();
        m_basicShapes[1].reset();
        m_basicShapes[2].reset();
    }

    std::shared_ptr<ElysiaModel::LoadedModel> ModelManager::LoadStaticModel(
        const std::wstring& filePath,
        float scale)
    {
        const size_t fileHash = MakeModelCacheKey(filePath, true, true, false, false, scale);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_modelCache.find(fileHash);
            if (it != m_modelCache.end())
            {
                if (auto sharedModel = it->second.lock())
                {
                    return sharedModel;
                }
                else
                {
                    m_modelCache.erase(it);
                }
            }
        }

        std::shared_ptr<ElysiaModel::LoadedModel> sharedModel = LoadModelFromDisk(filePath,
            true,
            true,
            false,
            false,
            scale);

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_modelCache.find(fileHash);
            if (it != m_modelCache.end())
            {
                if (auto sharedModel = it->second.lock())
                {
                    return sharedModel;
                }
            }

            m_modelCache.emplace(fileHash, sharedModel);
            return sharedModel;
        }

    }

    std::unique_ptr<ElysiaModel::LoadedModel> ModelManager::LoadModelFromDisk(
        const std::wstring& filePath,
        bool bInvertTexcoordY,
        bool bImportMeshes,
        bool bImportSkeletons,
        bool bImportAnimations,
        float scale)
    {
        std::unique_ptr<ElysiaModel::LoadedModel> loadedModel = std::make_unique<
            ElysiaModel::LoadedModel>();

        ElysiaModel::ModelImportSettings settings;
        settings.invertTexcoordY = bInvertTexcoordY;
        settings.importMeshes = bImportMeshes;
        settings.importSkeletons = bImportSkeletons;
        settings.importAnimations = bImportAnimations;
        settings.scale = scale;

        loadedModel->sourcePath = filePath;
        if (ElysiaModel::ModelCache::TryLoad(filePath, settings, *loadedModel))
        {
            ElysiaModel::MaterialOverrides::Apply(*loadedModel);
            ElysiaModel::BindMaterialTextures(*loadedModel);
            ElysiaModel::CreateGpuResources(*loadedModel);
            return loadedModel;
        }

        if (!ElysiaModel::ParseGLTFToCPU(filePath,
                                         bInvertTexcoordY,
                                         bImportMeshes,
                                         bImportSkeletons,
                                         bImportAnimations,
                                         scale,
                                         *loadedModel))
        {
            return loadedModel;
        }

        if (!ElysiaModel::ModelCache::TrySave(filePath, settings, *loadedModel))
        {
            ElysiaHelper::Log::Warn("ModelCache: write failed, continuing without disk cache.");
        }

        ElysiaModel::MaterialOverrides::Apply(*loadedModel);
        ElysiaModel::BindMaterialTextures(*loadedModel);
        ElysiaModel::CreateGpuResources(*loadedModel);
        return loadedModel;
    }

    namespace
    {
        using ElysiaModel::IndexType;
        using ElysiaModel::LoadedMaterial;
        using ElysiaModel::LoadedModel;
        using ElysiaModel::MeshVertex;

        constexpr float kHalfExtent = 0.5f;
        constexpr float kSphereRadius = 0.5f;
        constexpr uint32_t kSphereSegments = 24;
        constexpr uint32_t kSphereRings = 16;

        void AddVertex(LoadedModel& model,
                       const Vector3& position,
                       const Vector3& normal,
                       const Vector2& uv,
                       const Vector3& tangent)
        {
            MeshVertex vertex{};
            vertex.Position = position;
            vertex.Normal = normal;
            vertex.UV = uv;
            vertex.Tangent = Vector4(tangent.x, tangent.y, tangent.z, 1.f);
            vertex.Color = Vector4(1.f, 1.f, 1.f, 1.f);
            model.vertices.push_back(vertex);
            model.aabbMin = Vector3::Min(model.aabbMin, position);
            model.aabbMax = Vector3::Max(model.aabbMax, position);
        }

        void AddTriangle(LoadedModel& model, uint32_t a, uint32_t b, uint32_t c)
        {
            model.indices.push_back(a);
            model.indices.push_back(b);
            model.indices.push_back(c);
        }

        // p0..p3 are CCW when viewed from outside. (p1-p0)×(p2-p0) == normal.
        void AddFace(LoadedModel& model,
                     const Vector3& normal,
                     const Vector3& tangent,
                     const Vector3& p0,
                     const Vector3& p1,
                     const Vector3& p2,
                     const Vector3& p3)
        {
            const uint32_t base = static_cast<uint32_t>(model.vertices.size());
            AddVertex(model, p0, normal, Vector2(0.f, 0.f), tangent);
            AddVertex(model, p1, normal, Vector2(1.f, 0.f), tangent);
            AddVertex(model, p2, normal, Vector2(1.f, 1.f), tangent);
            AddVertex(model, p3, normal, Vector2(0.f, 1.f), tangent);
            AddTriangle(model, base, base + 1, base + 2);
            AddTriangle(model, base, base + 2, base + 3);
        }

        void BuildCube(LoadedModel& model)
        {
            const float h = kHalfExtent;
            AddFace(model, Vector3(0.f, 1.f, 0.f), Vector3(1.f, 0.f, 0.f),
                    Vector3(-h, h, h), Vector3(h, h, h), Vector3(h, h, -h), Vector3(-h, h, -h));
            AddFace(model, Vector3(0.f, -1.f, 0.f), Vector3(1.f, 0.f, 0.f),
                    Vector3(-h, -h, -h), Vector3(h, -h, -h), Vector3(h, -h, h), Vector3(-h, -h, h));
            AddFace(model, Vector3(0.f, 0.f, 1.f), Vector3(1.f, 0.f, 0.f),
                    Vector3(-h, -h, h), Vector3(h, -h, h), Vector3(h, h, h), Vector3(-h, h, h));
            AddFace(model, Vector3(0.f, 0.f, -1.f), Vector3(-1.f, 0.f, 0.f),
                    Vector3(h, -h, -h), Vector3(-h, -h, -h), Vector3(-h, h, -h), Vector3(h, h, -h));
            AddFace(model, Vector3(1.f, 0.f, 0.f), Vector3(0.f, 0.f, -1.f),
                    Vector3(h, -h, h), Vector3(h, -h, -h), Vector3(h, h, -h), Vector3(h, h, h));
            AddFace(model, Vector3(-1.f, 0.f, 0.f), Vector3(0.f, 0.f, 1.f),
                    Vector3(-h, -h, -h), Vector3(-h, -h, h), Vector3(-h, h, h), Vector3(-h, h, -h));
        }

        void BuildPlane(LoadedModel& model)
        {
            const float h = kHalfExtent;
            AddFace(model, Vector3(0.f, 1.f, 0.f), Vector3(1.f, 0.f, 0.f),
                    Vector3(-h, 0.f, h), Vector3(h, 0.f, h), Vector3(h, 0.f, -h), Vector3(-h, 0.f, -h));
        }

        void BuildSphere(LoadedModel& model)
        {
            const float r = kSphereRadius;
            const uint32_t cols = kSphereSegments + 1;
            for (uint32_t lat = 0; lat <= kSphereRings; ++lat)
            {
                const float v = static_cast<float>(lat) / static_cast<float>(kSphereRings);
                const float theta = v * DirectX::XM_PI;
                const float sinTheta = std::sinf(theta);
                const float cosTheta = std::cosf(theta);
                for (uint32_t lon = 0; lon <= kSphereSegments; ++lon)
                {
                    const float u = static_cast<float>(lon) / static_cast<float>(kSphereSegments);
                    const float phi = u * DirectX::XM_2PI;
                    const float sinPhi = std::sinf(phi);
                    const float cosPhi = std::cosf(phi);
                    const Vector3 normal(sinTheta * sinPhi, cosTheta, sinTheta * cosPhi);
                    const Vector3 position = normal * r;
                    Vector3 tangent(-cosPhi, 0.f, sinPhi);
                    if (tangent.LengthSquared() < 1.0e-8f)
                        tangent = Vector3(1.f, 0.f, 0.f);
                    else
                        tangent.Normalize();
                    AddVertex(model, position, normal, Vector2(u, v), tangent);
                }
            }

            for (uint32_t lat = 0; lat < kSphereRings; ++lat)
            {
                for (uint32_t lon = 0; lon < kSphereSegments; ++lon)
                {
                    const uint32_t i0 = lat * cols + lon;
                    const uint32_t i1 = i0 + 1;
                    const uint32_t i2 = i0 + cols;
                    const uint32_t i3 = i2 + 1;
                    if (lat == 0)
                        AddTriangle(model, i0, i2, i3);
                    else if (lat + 1 == kSphereRings)
                        AddTriangle(model, i0, i2, i1);
                    else
                    {
                        AddTriangle(model, i0, i2, i1);
                        AddTriangle(model, i1, i2, i3);
                    }
                }
            }
        }

        void FinalizeBasicShape(LoadedModel& model, const char* name)
        {
            model.name = name;
            model.scale = 1.f;
            model.sourcePath.clear();

            LoadedModel::Mesh mesh;
            mesh.name = name;
            mesh.materialIndex = 0;
            mesh.numVertices = static_cast<UINT32>(model.vertices.size());
            mesh.numIndices = static_cast<UINT32>(model.indices.size());
            mesh.vtxOffset = 0;
            mesh.idxOffset = 0;
            mesh.indexType = IndexType::Index32Bit;
            mesh.aabbMin = model.aabbMin;
            mesh.aabbMax = model.aabbMax;
            mesh.logicalCenter = (model.aabbMin + model.aabbMax) * 0.5f;
            model.meshes.push_back(std::move(mesh));

            LoadedMaterial material{};
            material.name = "BasicShapeMaterial";
            material.alpha = LoadedMaterial::Alpha::Opaque;
            material.albedoFactor = Vector3(0.8f, 0.8f, 0.8f);
            material.metallicFactor = 0.f;
            material.roughnessFactor = 0.5f;
            material.specularFactor = 0.5f;
            material.shadingModelID = 1;
            material.opacity = 1.f;
            model.materials.push_back(std::move(material));
        }

        std::unique_ptr<LoadedModel> CreateBasicShapeModel(BasicShapeType type)
        {
            auto model = std::make_unique<LoadedModel>();
            const char* name = GetBasicShapeTypeName(type);
            switch (type)
            {
            case BasicShapeType::Sphere:
                BuildSphere(*model);
                break;
            case BasicShapeType::Plane:
                BuildPlane(*model);
                break;
            default:
                BuildCube(*model);
                break;
            }
            FinalizeBasicShape(*model, name);
            return model;
        }
    }

    std::shared_ptr<ElysiaModel::LoadedModel> ModelManager::GetOrCreateBasicShape(uint8_t type)
    {
        const auto shapeType = static_cast<BasicShapeType>(type);
        if (!IsValidBasicShapeType(shapeType))
            return nullptr;

        const size_t index = static_cast<size_t>(type);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_basicShapes[index])
                return m_basicShapes[index];
        }

        std::shared_ptr<ElysiaModel::LoadedModel> model = CreateBasicShapeModel(shapeType);
        if (!model || model->meshes.empty() || model->vertices.empty() || model->indices.empty())
        {
            ElysiaHelper::Log::Warn("ModelManager: failed to build basic shape \"%s\".",
                                    GetBasicShapeTypeName(shapeType));
            return nullptr;
        }

        ElysiaModel::BindMaterialTextures(*model);
        if (!ElysiaModel::CreateGpuResources(*model))
        {
            ElysiaHelper::Log::Warn("ModelManager: GPU upload failed for basic shape \"%s\".",
                                    GetBasicShapeTypeName(shapeType));
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_basicShapes[index])
            return m_basicShapes[index];
        m_basicShapes[index] = model;
        return model;
    }
}