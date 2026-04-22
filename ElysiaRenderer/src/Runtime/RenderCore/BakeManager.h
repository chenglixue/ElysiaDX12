#pragma once
#include "Programs/Helper.h"
#include "Programs/IManager.h"

namespace ElysiaCore
{
    class DX12Device;
}

namespace ElysiaRenderer
{
    class Material;
}

namespace ElysiaRenderer
{
    enum class EBakeTaskFlags : UINT
    {
        None = 0,
        SSSLut = 1 << 0,
        SSSNDFLut = 1 << 1,
        All = 0xFFFFFFFF
    };

    class BakeManager : IManager
    {
    public:
        BakeManager() = default;
        BakeManager(const BakeManager& rhs) = delete;
        BakeManager& operator=(BakeManager& rhs) = delete;
        BakeManager(BakeManager&& rhs) = default;
        ~BakeManager();

        static BakeManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new BakeManager());
                           });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;

        void RequestMasks(EBakeTaskFlags flags);
        bool ConsumeMasks(EBakeTaskFlags flagToConsume);

    private:
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<BakeManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        std::atomic<UINT> m_pendingTasks{0};
    };
}