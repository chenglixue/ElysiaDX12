#pragma once
#include "IManager.h"
#include "IUpdate.h"
#include "lib/DX12/DX12Camera.h"

namespace ElysiaRenderer
{
	class CameraManager : public IManager, IUpdate
	{
	public:
		CameraManager() = default;
		CameraManager(const CameraManager& rhs) = delete;
		CameraManager& operator=(CameraManager& rhs) = delete;
		CameraManager(CameraManager&& rhs) = default;
		~CameraManager();

		virtual void Init() override;
		virtual void Update() override;
		virtual void Destory() override;

		DX12Camera* GetMainCamera()
		{
			return m_mainCamera.get();
		}

		void CreateMainCamera(Vector3 position, float aspectRatio, float fovy, float nearZ, float farZ);

	private:

		std::unique_ptr<DX12Camera> m_mainCamera = nullptr;
	};
}