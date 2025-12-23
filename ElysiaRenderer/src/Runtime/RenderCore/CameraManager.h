#pragma once
#include "Programs/IManager.h"
#include "Programs/IUpdate.h"
#include "DX12Camera.h"

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

		static CameraManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new CameraManager());
				});

			return *m_instance;
		}

		virtual void Init(ElysiaCore::DX12Device* pDevice) override;
		virtual void Update() override;
		virtual void Destory() override;

		DX12Camera* GetMainCamera()
		{
			return m_mainCamera.get();
		}

		void CreateMainCamera(Vector3 position, float aspectRatio, float fovy, float nearZ, float farZ);

	private:
		static std::unique_ptr<CameraManager> m_instance;
		static std::once_flag m_initInstanceFlag;
		ElysiaCore::DX12Device* m_pDevice = nullptr;
		
		std::unique_ptr<DX12Camera> m_mainCamera = nullptr;
	};
}