#pragma once
#include "stdafx.h"
#include "IManager.h"
#include "DX12Model.h"

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

	private:
		std::vector<DX12Model> m_models{};
	};
}