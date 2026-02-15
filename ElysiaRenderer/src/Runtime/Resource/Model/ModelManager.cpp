#include "stdafx.h"
#include "ModelManager.h"

#include "AssimpLoader.h"
#include "Runtime/Resource/Model/AssimpLoader.h"
#include "Runtime/Resource/Model/LoadedModel.h"

namespace ElysiaRenderer
{
    std::unique_ptr<ModelManager> ModelManager::m_instance;
    std::once_flag ModelManager::m_initInstanceFlag;
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
        std::lock_guard<std::mutex> lock(m_mutex);

        auto fileHash = xxh::GetHash(filePath);
        if (m_modelCache.count(fileHash))
        {
            if (auto sharedModel = m_modelCache[fileHash].lock())
            {
                return sharedModel;
            }
            else
            {
                m_modelCache.erase(fileHash);
            }
        }

        std::shared_ptr<ElysiaModel::LoadedModel> sharedModel = LoadModelFromDisk(filePath,
                                                                                  true,
                                                                                  true,
                                                                                  false,
                                                                                  false,
                                                                                  scale);

        m_modelCache.emplace(fileHash, sharedModel);
        return sharedModel;
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
        ElysiaModel::LoadGLTFModel(filePath,
                                   bInvertTexcoordY,
                                   bImportMeshes,
                                   bImportSkeletons,
                                   bImportAnimations,
                                   scale,
                                   *loadedModel);

        return loadedModel;
    }
}