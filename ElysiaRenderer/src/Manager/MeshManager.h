#pragma once
#include "IManager.h"

namespace ElysiaRenderer
{
	class MeshManager : public IManager
	{
	public:
		MeshManager() = default;
		MeshManager(const MeshManager& rhs) = delete;
		MeshManager& operator=(MeshManager& rhs) = delete;
		MeshManager(MeshManager&& rhs) = default;
		~MeshManager();

		static MeshManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new MeshManager());
				});

			return *m_instance;
		}

		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;

		void LoadModel(const std::vector<LPCWSTR>& modelPaths);

	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<MeshManager> m_instance;
		static std::once_flag m_initInstanceFlag;
		
		std::vector<LPCWSTR>	m_modelPaths;
	};
}