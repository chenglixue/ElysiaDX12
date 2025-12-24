#include "stdafx.h"
#include "CameraManager.h"

#include "Runtime/Engine/ECS/Transform.h"

namespace ElysiaRenderer
{
	std::unique_ptr<CameraManager> CameraManager::m_instance;
	std::once_flag CameraManager::m_initInstanceFlag;
	
	CameraManager::~CameraManager()
	{
		Destory();
	}

	void CameraManager::Init(ElysiaCore::DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
	}

	void CameraManager::Update(const ElysiaEngine::FrameContext& context)
	{
		m_frameID = context.frameID;
		m_frameIndex = context.frameIndex;
	}

	void CameraManager::Destory()
	{

	}

	void CameraManager::CreateMainCamera(Vector3 position, float aspectRatio, float fovy, float nearZ, float farZ)
	{
		Transform transform
		{
			.position = position
		};

		if (m_mainCamera == nullptr)
		{
			m_mainCamera = std::make_unique<DX12Camera>(transform, aspectRatio, fovy, nearZ, farZ);
		}
		else
		{
			if (m_mainCamera->m_transform == transform && m_mainCamera->GetAspect() == aspectRatio
				&& m_mainCamera->GetFOVY() == fovy && m_mainCamera->GetNearZ() == nearZ && m_mainCamera->GetFarZ() == farZ)
			{
				return;
			}
			m_mainCamera.reset();
			m_mainCamera = std::make_unique<DX12Camera>(transform, aspectRatio, fovy, nearZ, farZ);
		}
	}
}