#pragma once
#include "Programs/Helper.h"
#include "ThirdParty/FreesyncHDR.h"

namespace ElysiaCore
{
    class DX12Device;
    class DX12TextureResource;
}

namespace ElysiaCore
{
    using namespace CAULDRON_DX12;
    class SwapChain
    {
    public:
        SwapChain();
        ~SwapChain();
        void OnCreate(DX12Device *pDevice, HWND hWnd);
        void OnDestroy();

        void OnCreateWindowSizeDependentResources(uint32_t dwWidth, uint32_t dwHeight, bool bVSyncOn, DisplayMode displayMode = DISPLAYMODE_SDR, bool disableLocalDimming = false);
        void OnDestroyWindowSizeDependentResources();

        void SetFullScreen(bool fullscreen);
        const bool GetFullScreen();

        bool IsModeSupported(DisplayMode displayMode);
        void EnumerateDisplayModes(std::vector<DisplayMode> *pModes, std::vector<const char *> *pNames = NULL);

        void Present();
        void WaitForSwapChain();

        DX12TextureResource& GetCurrBackBuffer();
        ID3D12Resource *GetCurrentBackBufferResource();
        D3D12_CPU_DESCRIPTOR_HANDLE *GetCurrentBackBufferRTV();
        DXGI_FORMAT GetFormat();
        DisplayMode GetDisplayMode();
        
    private:
        void CreateRTV();

        HWND m_hWnd = NULL;
        const uint32_t m_BackBufferCount = ElysiaHelper::NUM_FRAMES_IN_FLIGHT;

        DX12Device *m_pDevice = NULL;
        IDXGIFactory6 *m_pFactory = NULL;
        IDXGISwapChain4 *m_pSwapChain = NULL;

        DisplayMode m_displayMode = DISPLAYMODE_SDR;
        DXGI_FORMAT m_swapChainFormat = DXGI_FORMAT_UNKNOWN;

        std::array<std::unique_ptr<DX12TextureResource>, ElysiaHelper::NUM_BACK_BUFFERS> m_backBuffers;

        DXGI_SWAP_CHAIN_DESC1 m_descSwapChain = {};

        bool m_bVSyncOn = false;

        BOOL m_bTearingSupport = false;
        BOOL m_bIsFullScreenExclusive = false;
    };
}

