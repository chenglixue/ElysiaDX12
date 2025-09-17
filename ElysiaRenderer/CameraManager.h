#pragma once
#include "IManager.h"
#include "DX12Camera.h"

namespace ElysiaRenderer
{
	class CameraManager : public IManager
	{
	public:
		CameraManager();
		CameraManager(const CameraManager& rhs) = delete;
		CameraManager& operator=(CameraManager& rhs) = delete;
		CameraManager(CameraManager&& rhs) = default;
		~CameraManager();

		DX12Camera* GetMainCamera()
		{
			return m_mainCamera.get();
		}

	private:
		void CreateMainCamera();
		std::unique_ptr<DX12Camera> m_mainCamera;
	};
}