#pragma once
#include "FrameContext.h"
#include "ThirdParty/FreesyncHDR.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/SwapChain.h"
#include "ThirdParty/imgui/imgui.h"

namespace ElysiaEngine
{
    class ElysiaCore::DX12Device;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ElysiaEngine
{
    using namespace CAULDRON_DX12;
    
    class FrameworkWindows
    {
    public:
        FrameworkWindows(eastl::wstring name);
        virtual ~FrameworkWindows() {};

        // Client (Sample) application interface
        virtual void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight) = 0;
        virtual void OnCreate() = 0;
        virtual void OnDestroy() = 0;
        virtual void OnRender() = 0;
        virtual bool OnEvent(MSG msg) = 0;
        virtual void OnResize() = 0;
        virtual void OnUpdateDisplay() = 0;

        // Device & swapchain management
        void DeviceInit(HWND WindowsHandle);
        void DeviceShutdown();
        FrameContext BeginFrame();
        void EndFrame();
        void Present();

        // Fullscreen management & window events are handled by Cauldron instead of the client applications.
        void ToggleFullScreen();
        void HandleFullScreen();
        void OnActivate(bool windowActive);
        void OnWindowMove();
        void UpdateDisplay(int displayMode, bool disableLocalDimming);
        void OnResize(uint32_t Width, uint32_t Height, bool forceManulResize);
        void HandleResize(uint32_t Width, uint32_t Height) { OnResize(Width, Height, m_forceManualResize); }
		
        // Getters
        inline eastl::wstring     GetName() const { return m_Name; }
        inline uint32_t           GetWidth() const { return m_Width; }
        inline uint32_t           GetHeight() const { return m_Height; }
        inline CAULDRON_DX12::DisplayMode        GetCurrentDisplayMode() const { return m_currentDisplayModeNamesIndex; }
        inline size_t             GetNumDisplayModes() const { return m_displayModesAvailable.size(); }
        inline const char* const* GetDisplayModeNames() const { return &m_displayModesNamesAvailable[0]; }
        inline bool               GetLocalDimmingDisableState() const { return m_disableLocalDimming; }
        
    protected:
        eastl::wstring m_Name; // sample application name
        int m_Width ;  // application window dimensions
        int m_Height;  // application window dimensions
        UINT m_frameID;
        UINT64 m_frameIndex;
        
        // Simulation management
        double  m_lastFrameTime;
        double  m_deltaTime;

        // Device management
        HWND   m_windowHwnd;
        ElysiaCore::DX12Device* m_pDevice;
        bool   m_stablePowerState;
        bool   m_isCpuValidationLayerEnabled;
        bool   m_isGpuValidationLayerEnabled;
        bool   m_initializeAGS;

        // Swapchain management
        ElysiaCore::SwapChain         m_swapChain;
        bool              m_VsyncEnabled;
        PresentationMode  m_fullscreenMode;
        PresentationMode  m_previousFullscreenMode;

        // Display management
        HMONITOR                  m_monitor;
        bool                      m_FreesyncHDROptionEnabled;
        DisplayMode               m_currentDisplayMode;
        DisplayMode               m_previousDisplayModeNamesIndex;
        DisplayMode               m_currentDisplayModeNamesIndex;
        std::vector<DisplayMode>  m_displayModesAvailable;
        std::vector<const char*>  m_displayModesNamesAvailable;
        bool                      m_disableLocalDimming;
        bool                      m_forceManualResize;

        // System info
        struct SystemInfo
        {
            std::string mCPUName = "UNAVAILABLE";
            std::string mGPUName = "UNAVAILABLE";
            std::string mGfxAPI  = "UNAVAILABLE";
        };
        SystemInfo  m_systemInfo;
    };

    int RunFramework(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow, FrameworkWindows *pFramework);
    void SetFullscreen(HWND hWnd, bool fullscreen);
}
