#include "stdafx.h"
#include "ElysiaFrame.h"


#include "Editor/IMGUIHelper.h"
#include "Editor/UserData.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/LightManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/TextureManager.h"

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;
    
    static void ToggleBool(bool& b) { b = !b; }
    
    ElysiaFrame::ElysiaFrame(eastl::wstring name) :
        FrameworkWindows(name)
    {
        
        m_time = 0;
        m_bPlay = true;
        
#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
        Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
        if (FAILED(initialize))
        {
            
        }
            // error
#else
        HRESULT hr = ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        if (FAILED(hr))
        {
            
        }
            // error
#endif
    }

    void ElysiaFrame::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight)
    {
        *pWidth = 1920;
        *pHeight = 1080;

        m_VsyncEnabled = false;
        m_bIsBenchmarking = false;
        m_isCpuValidationLayerEnabled = false;
        m_isGpuValidationLayerEnabled = false;
        m_stablePowerState = false;

    }

    void ElysiaFrame::OnCreate()
    {
        DeSerializeUserData();

        BufferManager::GetInstance().Init(m_pDevice);
        TextureManager::GetInstance().Init(m_pDevice);
        RenderTargetManager::GetInstance().Init(m_pDevice);
        CameraManager::GetInstance().Init(m_pDevice);
        LightManager::GetInstance().Init(m_pDevice);
        PSOManager::GetInstance().Init(m_pDevice);
        
        m_pRenderer = new ElysiaRenderer::Renderer();
        m_pRenderer->OnCreate(m_pDevice, &m_swapChain);

        ElysiaEditor::ImGUI_Init(m_windowHwnd, m_pDevice, m_swapChain);

        OnResize();
        OnUpdateDisplay();

        
    }

    void ElysiaFrame::OnDestroy()
    {
        ElysiaEditor::ImGUI_Shutdown();

        m_pDevice->WaitForIdle();
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnDestory();
        delete m_pRenderer;
    }

    void ElysiaFrame::OnResize()
    {
        if (m_Width && m_Height && m_pRenderer)
        {
            m_pRenderer->OnDestroyWindowSizeDependentResources();
            m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
        }
    }

    void ElysiaFrame::OnUpdateDisplay()
    {
        if (m_pRenderer)
        {
            m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
        }
    }

    bool ElysiaFrame::OnEvent(MSG msg)
    {
        if (ElysiaEditor::ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
            return true;

        // handle function keys (F1, F2...) here, rest of the input is handled
        // by imGUI later in HandleInput() function
        const WPARAM& KeyPressed = msg.wParam;
        switch (msg.message)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            /* WINDOW TOGGLES */
            if (KeyPressed == VK_F1) ToggleBool(m_UIState.bShowControlsWindow);
            if (KeyPressed == VK_F2) ToggleBool(m_UIState.bShowProfilerWindow);
            break;
        }

        return true;
    }

    void ElysiaFrame::OnRender()
    {
        auto frameContext = BeginFrame();
        ImGUI_UpdateIO();
        ImGUI_NewFrame();

        std::vector<RenderItem> renderList;
        if (m_loadingScene)
        {
            static int loadingStage = 0;
            SceneManager::GetInstance().LoadScene(renderList);
            if (loadingStage == 0)
            {
                m_time = 0;
                m_loadingScene = false;
            }
        }
        else if (m_bIsBenchmarking)
        {
            // Benchmarking takes control of the time, and exits the app when the animation is done
            std::vector<TimeStamp> timeStamps = m_pRenderer->GetTimingValues();
            // m_time = BenchmarkLoop(timeStamps, &m_camera, m_pRenderer->GetScreenshotFileName());
        }
        else
        {
            OnUpdate();
            BufferManager::GetInstance().Update(frameContext);
        }

        if (!m_loadingScene)
        {
            m_pRenderer->OnRender(frameContext);
        }
        EndFrame();
    }

    void ElysiaFrame::OnUpdate()
    {
        ImGuiIO& io = ImGui::GetIO();

        //If the mouse was not used by the GUI then it's for the camera
        if (io.WantCaptureMouse)
        {
            io.MouseDelta.x = 0;
            io.MouseDelta.y = 0;
            io.MouseWheel = 0;
        }

        // Keyboard & Mouse
        HandleInput(io);
    }

    void ElysiaFrame::HandleInput(const ImGuiIO& io)
    {
        auto fnIsKeyTriggered = [&io](char key) { return io.KeysDown[key] && io.KeysDownDuration[key] == 0.0f; };

        // Handle Keyboard/Mouse input here

        /* MAGNIFIER CONTROLS */
        if (fnIsKeyTriggered('L'))                       m_UIState.ToggleMagnifierLock();
        if (fnIsKeyTriggered('M') || io.MouseClicked[2]) ToggleBool(m_UIState.bUseMagnifier); // middle mouse / M key toggles magnifier

        if (io.MouseClicked[1] && m_UIState.bUseMagnifier) // right mouse click
            m_UIState.ToggleMagnifierLock();

        if (fnIsKeyTriggered('R'))
            m_UIState.ResetLPMSceneDefaults();
    }

    //--------------------------------------------------------------------------------------
    //
    // WinMain
    //
    //--------------------------------------------------------------------------------------
    int WINAPI WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
    {
        eastl::wstring name(L"Elysia Engine");

        return RunFramework(hInstance, lpCmdLine, nCmdShow, new ElysiaEngine::ElysiaFrame(name));
    }
}
