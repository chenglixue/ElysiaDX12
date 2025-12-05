#include "stdafx.h"
#include "MeshManager.h"

namespace ElysiaRenderer
{
	std::unique_ptr<MeshManager> MeshManager::m_instance;
	std::once_flag MeshManager::m_initInstanceFlag;
	
	MeshManager::~MeshManager()
	{
		Destory();
	}

	void MeshManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
	}

	void MeshManager::Destory()
	{

	}

	void MeshManager::LoadModel(const std::vector<LPCWSTR>& modelPaths)
	{

	}
}