#include "stdafx.h"
#include "CameraManager.h"

#include "Runtime/Engine/ECS/Transform.h"
#include "DX12Camera.h"
#include "Runtime/Engine/FrameContext.h"


namespace ElysiaRenderer
{
    std::unique_ptr<CameraManager> CameraManager::m_instance;
    std::once_flag CameraManager::m_initInstanceFlag;

    CameraManager::CameraManager()
    {

    }
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

    void CameraManager::CreateMainCamera(Vector3 position,
                                         float aspectRatio,
                                         float fovy,
                                         float nearZ,
                                         float farZ)
    {
        if (m_mainCamera == nullptr)
        {
            m_mainCamera = std::make_unique<FirstPersonCamera>(nearZ, farZ, aspectRatio, fovy);
        }
        else
        {
            m_mainCamera.reset();
            m_mainCamera = std::make_unique<FirstPersonCamera>(nearZ, farZ, aspectRatio, fovy);

        }
        m_mainCamera->SetPosition(position);
        m_mainCamera->SetRotation(0, 0.7071f, 0);
    }
}