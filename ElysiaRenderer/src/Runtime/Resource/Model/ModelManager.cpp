#include "stdafx.h"
#include "ModelManager.h"

#include "AssimpLoader.h"
#include "Runtime/Resource/Model/AssimpLoader.h"
#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Resource/Model/ModelCache.h"
#include "Programs/Hash.h"
#include "Programs/Log.h"

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

    void ModelManager::Destory()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_modelCache.begin();
        while (it != m_modelCache.end())
        {
            it = m_modelCache.erase(it);
        }
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

        if (ElysiaModel::ModelCache::TryLoad(filePath, settings, *loadedModel))
        {
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

        ElysiaModel::BindMaterialTextures(*loadedModel);
        ElysiaModel::CreateGpuResources(*loadedModel);
        return loadedModel;
    }
}