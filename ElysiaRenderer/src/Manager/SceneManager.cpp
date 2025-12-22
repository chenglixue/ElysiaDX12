#include "stdafx.h"
#include "SceneManager.h"

namespace ElysiaRenderer
{
    std::unique_ptr<SceneManager> m_instance;
    std::once_flag m_initInstanceFlag;

    SceneManager::~SceneManager()
    {
        Destory();
    }

    void SceneManager::Init(DX12Device* pDevice)
    {
        m_pDevice = pDevice;
    }
    void SceneManager::Destory()
    {
        
    }

    
}