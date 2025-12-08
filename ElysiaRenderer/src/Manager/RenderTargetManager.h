#pragma once
#include "lib/Utility/Helper.h"
#include "IManager.h"

namespace ElysiaRenderer
{
    class DX12Device;
    class RenderTexture;
}

namespace ElysiaRenderer
{
    class RenderTargetManager : public IManager
    {
    public:
        RenderTargetManager() = default;
        RenderTargetManager(DX12Device* pDevice);
        RenderTargetManager(const RenderTargetManager& rhs) = delete;
        RenderTargetManager& operator=(RenderTargetManager& rhs) = delete;
        RenderTargetManager(RenderTargetManager&& rhs) = default;
        ~RenderTargetManager();

        static RenderTargetManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag, []() {
                m_instance.reset(new RenderTargetManager());
                });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;
        
        RenderTexture* GetRenderTexture(const std::string& name) const;
        RenderTexture* GetRenderTexture(size_t nameHash) const;

        RenderTexture* CreateRenderTexture(
        UINT64 width, 
        UINT64 height,
        DXGI_FORMAT format,
        const std::string& name);
        
        RenderTexture* CreateRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool isDepth,
        const std::string& name);

        RenderTexture* CreateRWRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool enableRandomWrite,
        const std::string& name);
        
    private:
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<RenderTargetManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        std::unordered_map<size_t, std::unique_ptr<RenderTexture>> m_renderTextures;
    };
}

