#include "stdafx.h"
#include "ModelManager.h"
#include "lib/Model/AssimpLoader.h"

namespace ElysiaRenderer
{
	std::unique_ptr<ModelManager> ModelManager::m_instance;
	std::once_flag ModelManager::m_initInstanceFlag;
	ModelManager::~ModelManager()
	{
		Destory();
	}
	
	void ModelManager::Init(DX12Device* pDevice) 
	{
		m_pDevice = pDevice;
	}
	void ModelManager::Destory() 
	{
		
	}
	
	std::shared_ptr<ElysiaModel::LoadedModel> ModelManager::LoadModel(const wchar_t* filePath, float scale)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		auto fileHash = xxh::GetHash(WstringToString(filePath));
		if(m_modelCache.count(fileHash))
		{
			if(auto sharedModel = m_modelCache[fileHash].lock())
			{
				return sharedModel;
			}
		}
		
		auto newModel = LoadModelFromDisk(filePath, true, true, true, true, scale);
		
		std::shared_ptr<ElysiaModel::LoadedModel> sharedModel(newModel.release(), [this, fileHash](ElysiaModel::LoadedModel* ptr)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_modelCache.erase(fileHash);
			delete ptr;
		});
		
		m_modelCache.emplace(fileHash, sharedModel);
		return sharedModel;
	}
	
	std::unique_ptr<ElysiaModel::LoadedModel>  ModelManager::LoadModelFromDisk(const wchar_t* filePath, bool bInvertTexcoordY, bool bImportMeshes,
			bool bImportSkeletons, bool bImportAnimations, float scale)
	{
		std::unique_ptr<ElysiaModel::LoadedModel> loadedModel = std::make_unique<ElysiaModel::LoadedModel>();
		ElysiaModel::LoadModel(filePath, bInvertTexcoordY, bImportMeshes, bImportSkeletons, bImportAnimations, scale, *loadedModel);
		
		return loadedModel;
	}
}