#include "stdafx.h"
#include "RenderPassResourceManager.h"

namespace ElysiaRenderer
{
    std::unique_ptr<RenderPassResourceManager> RenderPassResourceManager::m_instance;
    std::once_flag RenderPassResourceManager::m_initInstanceFlag;

    RenderPassResourceManager::RenderPassResourceManager() = default;
    RenderPassResourceManager::~RenderPassResourceManager() = default;

    void RenderPassResourceManager::Destory()
    {

    }
    void RenderPassResourceManager::Init(ElysiaCore::DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
    }
}