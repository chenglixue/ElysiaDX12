#pragma once
#include "Programs/public/IManager.h"

namespace ElysiaModel
{
	struct LoadedModel;
}

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
		void FlushMaterialEdits();
		
		std::shared_ptr<ElysiaModel::LoadedModel> LoadStaticModel(const std::wstring& filePath, float scale);
		// Procedural UE BasicShapes stand-in. One GPU model per type, kept alive
		// for the process so placing/deleting actors does not rebuild buffers.
		std::shared_ptr<ElysiaModel::LoadedModel> GetOrCreateBasicShape(uint8_t type);
		
	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<ModelManager> m_instance;
		static std::once_flag m_initInstanceFlag;
		
		std::mutex m_mutex;
		std::unordered_map<size_t, std::weak_ptr<ElysiaModel::LoadedModel>> m_modelCache;
		std::shared_ptr<ElysiaModel::LoadedModel> m_basicShapes[3];
		
		std::unique_ptr<ElysiaModel::LoadedModel> LoadModelFromDisk(const std::wstring& filePath, bool bInvertTexcoordY, bool bImportMeshes,
			bool bImportSkeletons, bool bImportAnimations, float scale);
	};
}

