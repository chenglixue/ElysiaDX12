#pragma once
#include "Programs/IManager.h"
#include "Runtime/Model/LoadedModel.h"

namespace ElysiaRenderer
{
	class ModelManager : IManager
	{
	public:
		ModelManager() = default;
		ModelManager(const ModelManager& rhs) = delete;
		ModelManager& operator=(ModelManager& rhs) = delete;
		ModelManager(ModelManager&& rhs) = default;
		~ModelManager();
		
		static ModelManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []()
			{
				m_instance.reset(new ModelManager());
			});

			return *m_instance;
		}
		
		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;
		
		std::shared_ptr<ElysiaModel::LoadedModel> LoadStaticModel(const wchar_t* filePath, float scale);
		
	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<ModelManager> m_instance;
		static std::once_flag m_initInstanceFlag;
		
		std::mutex m_mutex;
		eastl::hash_map<size_t, std::weak_ptr<ElysiaModel::LoadedModel>> m_modelCache;
		
		std::unique_ptr<ElysiaModel::LoadedModel> LoadModelFromDisk(const wchar_t* filePath, bool bInvertTexcoordY, bool bImportMeshes,
			bool bImportSkeletons, bool bImportAnimations, float scale);
	};
}

