#include "stdafx.h"
#include "RenderTargetManager.h"

#include "RenderTextureUtility.h"
#include "Runtime/RenderCore//RenderTexture.h"

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
    void RenderTargetManager::Init(ElysiaCore::DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
    }
    
    RenderTexture* RenderTargetManager::GetRenderTexture(const std::string& name) const
    {
        auto nameHash = xxh::GetHash(name);
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
        auto nameHash = xxh::GetHash(name);
        if (m_renderTextures.contains(nameHash))
        {
            auto RT = GetRenderTexture(nameHash);
            if (width == RT->GetWidth() && height == RT->GetHeight() && format == RT->GetFormat())
            {
                return RT;
            }
            else
            {
                m_renderTextures.at(nameHash).reset();
            }
        }
        
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);

        auto newRT = std::make_unique<RenderTexture>();
        
        auto emplaceResult = m_renderTextures.try_emplace(xxh::GetHash(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(xxh::GetHash(name)).get();
    }
        
    RenderTexture* RenderTargetManager::CreateRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool isDepth,
        const std::string& name)
    {
        auto nameHash = xxh::GetHash(name);
        
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);
        desc.IsDepth = isDepth;

        if (m_renderTextures.contains(nameHash))
        {
            auto RT = GetRenderTexture(nameHash);
            if (width == RT->GetWidth() && height == RT->GetHeight() && format == RT->GetFormat())
            {
                return RT;
            }
            else
            {
                m_renderTextures.at(nameHash).reset();
            }
        }

        auto newRT = std::make_unique<RenderTexture>();
        auto emplaceResult = m_renderTextures.try_emplace(xxh::GetHash(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(xxh::GetHash(name)).get();
    }

    RenderTexture* RenderTargetManager::CreateRWRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool enableRandomWrite,
        const std::string& name)
    {
        auto nameHash = xxh::GetHash(name);
        if (m_renderTextures.contains(nameHash))
        {
            auto RT = GetRenderTexture(nameHash);
            if (width == RT->GetWidth() && height == RT->GetHeight() && format == RT->GetFormat())
            {
                return RT;
            }
            else
            {
                m_renderTextures.at(nameHash).reset();
            }
        }
        
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = stringToLPCWSTR(name);
        desc.EnableRandomWrite = enableRandomWrite;

        auto newRT = std::make_unique<RenderTexture>();
        auto emplaceResult = m_renderTextures.try_emplace(xxh::GetHash(name));
        if(emplaceResult.second)
        {
            newRT->Init(m_pDevice, desc);
            emplaceResult.first->second = std::move(newRT);
            
            return emplaceResult.first->second.get();
        }
        
        return m_renderTextures.at(xxh::GetHash(name)).get();
    }

}