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

		virtual void Init() override;
		virtual void Destory() override;

		void LoadModel(const std::vector<LPCWSTR>& modelPaths);

	private:
		std::vector<LPCWSTR>	m_modelPaths;
		//std::vector<DX12Model>	m_models{};
	};
}