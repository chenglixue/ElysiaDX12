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

    std::unique_ptr<RenderTexture> RenderTargetManager::CreateRenderTexture(
        UINT64 width, 
        UINT64 height,
        DXGI_FORMAT format,
        const wchar_t* name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;

        auto cameraDepthRT = std::make_unique<RenderTexture>();
        cameraDepthRT->Init(m_pDevice, desc);

        return cameraDepthRT;
    }
        
    std::unique_ptr<RenderTexture> RenderTargetManager::CreateRenderTexture(
    UINT64 width,
    UINT64 height,
    DXGI_FORMAT format,
    bool isDepth,
    const wchar_t* name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;
        desc.IsDepth = isDepth;

        auto cameraDepthRT = std::make_unique<RenderTexture>();
        cameraDepthRT->Init(m_pDevice, desc);

        return cameraDepthRT;
    }

    std::unique_ptr<RenderTexture> RenderTargetManager::CreateRWRenderTexture(
    UINT64 width,
    UINT64 height,
    DXGI_FORMAT format,
    bool enableRandomWrite,
    const wchar_t* name)
    {
        RenderTextureDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = format;
        desc.Name = name;
        desc.EnableRandomWrite = enableRandomWrite;

        auto cameraDepthRT = std::make_unique<RenderTexture>();
        cameraDepthRT->Init(m_pDevice, desc);

        return cameraDepthRT;
    }

}