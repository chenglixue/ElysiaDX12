#include "CameraManager.h"

namespace ElysiaRenderer
{
	CameraManager::CameraManager()
	{
		CreateMainCamera();
	}

	void CameraManager::CreateMainCamera()
	{
		if (m_mainCamera == nullptr)
		{
			m_mainCamera = std::unique_ptr<DX12Camera>();
		}
		else
		{
			m_mainCamera.reset();
			m_mainCamera = std::unique_ptr<DX12Camera>();
		}
	}

}