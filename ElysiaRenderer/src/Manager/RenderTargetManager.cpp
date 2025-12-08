#include "stdafx.h"
#include "RenderTargetManager.h"

#include "lib/Utility/RenderTexture.h"

namespace ElysiaRenderer
{
    std::unique_ptr<RenderTargetManager> RenderTargetManager::m_instance;
    std::once_flag RenderTargetManager::m_initInstanceFlag;

    RenderTargetManager::~RenderTargetManager()
    {
        
    }
    void RenderTargetManager::Destory() 
    {
        
    }
    void RenderTargetManager::Init(DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
    }
    
    RenderTexture* RenderTargetManager::GetRenderTexture(const std::string& name) const
    {
        auto nameHash = PropertyToID(name);
        if(m_renderTextures.contains(nameHash))
        {
            return m_renderTextures.at(nameHash).get();
        }
        
        assert("not find target render texture");
        
        return nullptr;
    }
    RenderTexture* RenderTargetManager::GetRenderTexture(size_t nameHash) const
    {
        if(m_renderTextures.contains(nameHash))
        {
            return m_renderTextures.at(nameHash).get();
        }
        
        assert("not find target render texture");
        
        return nullptr;
    }

    RenderTexture* RenderTargetManager::CreateRenderTexture(
        UINT64 width, 
        UINT64 height,
        DXGI_FORMAT format,
        const std::string& name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);

        auto newRT = std::make_unique<RenderTexture>();
        
        auto emplaceResult = m_renderTextures.try_emplace(PropertyToID(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(PropertyToID(name)).get();
    }
        
    RenderTexture* RenderTargetManager::CreateRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool isDepth,
        const std::string& name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);
        desc.IsDepth = isDepth;

        auto newRT = std::make_unique<RenderTexture>();
        auto emplaceResult = m_renderTextures.try_emplace(PropertyToID(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(PropertyToID(name)).get();
    }

    RenderTexture* RenderTargetManager::CreateRWRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool enableRandomWrite,
        const std::string& name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);
        desc.EnableRandomWrite = enableRandomWrite;

        auto newRT = std::make_unique<RenderTexture>();
        auto emplaceResult = m_renderTextures.try_emplace(PropertyToID(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(PropertyToID(name)).get();
    }

}