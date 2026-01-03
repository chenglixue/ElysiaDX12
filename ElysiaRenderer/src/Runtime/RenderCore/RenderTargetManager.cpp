#include "stdafx.h"
#include "RenderTargetManager.h"

#include "RenderTextureUtility.h"
#include "Runtime/RenderCore//RenderTexture.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    std::unique_ptr<RenderTargetManager> RenderTargetManager::m_instance;
    std::once_flag RenderTargetManager::m_initInstanceFlag;

    RenderTargetManager::RenderTargetManager() = default;
    RenderTargetManager::~RenderTargetManager() = default;

    void RenderTargetManager::Destory()
    {
        for (auto& RT : m_renderTextures)
        {
            if (RT.second)
            {
                RT.second->ShutDowm();
                RT.second.reset();
            }
        }
    }
    void RenderTargetManager::Init(ElysiaCore::DX12Device* pDevice)
    {
        assert(pDevice);
        m_pDevice = pDevice;
    }

    RenderTexture* RenderTargetManager::GetRenderTexture(const std::wstring& name) const
    {
        auto nameHash = xxh::GetHash(name);
        if (m_renderTextures.contains(nameHash))
        {
            return m_renderTextures.at(nameHash).get();
        }

        assert("not find target render texture");

        return nullptr;
    }
    RenderTexture* RenderTargetManager::GetRenderTexture(size_t nameHash) const
    {
        if (m_renderTextures.contains(nameHash))
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
        const std::wstring& name)
    {
        auto nameHash = xxh::GetHash(name);

        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;

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
                auto newRT = std::make_unique<RenderTexture>();
                newRT->Init(m_pDevice, desc);
                m_renderTextures[nameHash] = std::move(newRT);
            }
        }
        else
        {
            auto newRT = std::make_unique<RenderTexture>();
            newRT->Init(m_pDevice, desc);
            m_renderTextures.emplace(nameHash, std::move(newRT));
        }

        return m_renderTextures.at(nameHash).get();
    }

    RenderTexture* RenderTargetManager::CreateRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool isDepth,
        const std::wstring& name)
    {
        auto nameHash = xxh::GetHash(name);

        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;
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
                auto newRT = std::make_unique<RenderTexture>();
                newRT->Init(m_pDevice, desc);
                m_renderTextures[nameHash] = std::move(newRT);
            }
        }
        else
        {
            auto newRT = std::make_unique<RenderTexture>();
            newRT->Init(m_pDevice, desc);
            m_renderTextures.emplace(nameHash, std::move(newRT));
        }

        return m_renderTextures.at(nameHash).get();
    }

    RenderTexture* RenderTargetManager::CreateRWRenderTexture(
        UINT64 width,
        UINT64 height,
        DXGI_FORMAT format,
        bool enableRandomWrite,
        const std::wstring& name)
    {
        auto nameHash = xxh::GetHash(name);
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;
        desc.EnableRandomWrite = enableRandomWrite;

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
                auto newRT = std::make_unique<RenderTexture>();
                newRT->Init(m_pDevice, desc);
                m_renderTextures[nameHash] = std::move(newRT);
            }
        }
        else
        {
            auto newRT = std::make_unique<RenderTexture>();
            newRT->Init(m_pDevice, desc);
            m_renderTextures.emplace(nameHash, std::move(newRT));
        }

        return m_renderTextures.at(nameHash).get();
    }

}