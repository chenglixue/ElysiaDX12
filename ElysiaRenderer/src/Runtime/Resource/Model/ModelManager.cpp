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
        auto fileHash = xxh::GetHash(filePath);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_modelCache.find(fileHash);
            if (it != m_modelCache.end())
            {
                if (auto sharedModel = it->second.lock())
                {
                    return sharedModel; // 缓存命中，直接返回
                }
                else
                {
                    m_modelCache.erase(it); // 弱引用已失效，清理旧条目
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

            // 双重检查：在我们无锁加载的这段时间里，是不是有其他线程也加载了同一个模型并抢先写入了缓存？
            auto it = m_modelCache.find(fileHash);
            if (it != m_modelCache.end())
            {
                if (auto sharedModel = it->second.lock())
                {
                    // 别人抢先了一步！丢弃我们刚刚加载的重复结果，使用缓存中的实例
                    return sharedModel;
                }
            }

            // 安全写入缓存
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