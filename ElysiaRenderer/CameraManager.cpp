#include "CameraManager.h"

namespace ElysiaRenderer
{
	CameraManager::~CameraManager()
	{
		Destory();
	}

	void CameraManager::Init()
	{
		//CreateMainCamera(Vector3(0.0f, 3.0f, -10.0f), 16.f / 9.f, 3.14159f / 4.0f, 1.f, 300.f);
	}

	void CameraManager::Update()
	{

	}

	void CameraManager::Destory()
	{

	}

	void CameraManager::CreateMainCamera(Vector3 position, float aspectRatio, float fovy, float nearZ, float farZ)
	{
		Transform transform{};
		transform.m_position = position;

		if (m_mainCamera == nullptr)
		{
			m_mainCamera = std::make_unique<DX12Camera>(transform, aspectRatio, fovy, nearZ, farZ);
		}
		else
		{
			m_mainCamera.reset();
			m_mainCamera = std::make_unique<DX12Camera>(transform, aspectRatio, fovy, nearZ, farZ);
		}
	}
}