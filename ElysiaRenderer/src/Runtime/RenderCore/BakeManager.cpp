#include "stdafx.h"
#include "BakeManager.h"

#include "Runtime/Core/DX12Device.h"
#include "Material.h"
#include "Runtime/Core/DX12RootSignature.h"

namespace ElysiaRenderer
{
    std::unique_ptr<BakeManager> BakeManager::m_instance;
    std::once_flag BakeManager::m_initInstanceFlag;

    BakeManager::~BakeManager()
    {
        Destory();
    }

    void BakeManager::Init(ElysiaCore::DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
    }

    void BakeManager::Destory()
    {

    }

    void BakeManager::RequestMasks(EBakeTaskFlags flags)
    {
        m_pendingTasks.fetch_or(static_cast<UINT>(flags), std::memory_order_relaxed);
    }
    bool BakeManager::ConsumeMasks(EBakeTaskFlags flagToConsume)
    {
        UINT expected = m_pendingTasks.load(std::memory_order_relaxed);
        uint32_t flagVal = static_cast<uint32_t>(flagToConsume);

        if ((expected & flagVal) != 0)
        {
            m_pendingTasks.fetch_and(~flagVal, std::memory_order_relaxed);
            return true;
        }

    }
}