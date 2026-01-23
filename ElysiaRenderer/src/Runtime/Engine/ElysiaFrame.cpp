#include "stdafx.h"
#include "ElysiaFrame.h"


#include "Editor/IMGUIDrawer.h"
#include "Editor/IMGUIHelper.h"
#include "Editor/UserData.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/LightManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/TextureManager.h"
#include "Runtime/Resource/Model/ModelManager.h"

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;

    static void ToggleBool(bool& b)
    {
        b = !b;
    }

    ElysiaFrame::ElysiaFrame(std::wstring name) :
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
        if (!_CrtCheckMemory())
        {
            __debugbreak();
        }
        DeSerializeUserData();
        if (!_CrtCheckMemory())
        {
            __debugbreak();
        }

        BufferManager::GetInstance().Init(m_pDevice);
        TextureManager::GetInstance().Init(m_pDevice);
        RenderTargetManager::GetInstance().Init(m_pDevice);
        CameraManager::GetInstance().Init(m_pDevice);
        LightManager::GetInstance().Init(m_pDevice);
        PSOManager::GetInstance().Init(m_pDevice);
        SceneManager::GetInstance().Init(m_pDevice);

        m_pGraphicsContext = m_pDevice->CreateGraphicsContext();
        ElysiaEditor::ImGUI_Init(m_windowHwnd, m_pDevice, m_swapChain);
        m_pImGui->OnCreate(m_pDevice, &m_swapChain);

        m_pRenderer = new ElysiaRenderer::Renderer();
        m_pRenderer->OnCreate(m_pDevice, &m_swapChain, m_pGraphicsContext.get());

        OnResize();
        OnUpdateDisplay();

        m_loadingScene = true;
    }

    void ElysiaFrame::ReleaseResource()
    {
        ModelManager::GetInstance().Destory();
        RenderTargetManager::GetInstance().Destory();
    }

    void ElysiaFrame::OnDestroy()
    {
        m_pDevice->WaitForIdle();
        m_pGraphicsContext.release();
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
        // if (ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        //     return true;

        // handle function keys (F1, F2...) here, rest of the input is handled
        // by imGUI later in HandleInput() function
        const WPARAM& KeyPressed = msg.wParam;
        switch (msg.message)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            /* WINDOW TOGGLES */
            if (KeyPressed == VK_F1)
                ToggleBool(m_UIState.bShowControlsWindow);
            if (KeyPressed == VK_F2)
                ToggleBool(m_UIState.bShowProfilerWindow);
            break;
        }

        return true;
    }

    void ElysiaFrame::OnRender()
    {
        if (!_CrtCheckMemory())
        {
            __debugbreak();
        }
        auto frameContext = BeginFrame();
        m_pGraphicsContext->Reset();
        ImGUI_UpdateIO();
        ImGUI_NewFrame();

        if (m_loadingScene)
        {
            static UINT loadingStage = 0;
            SceneManager::GetInstance().LoadScene(loadingStage);
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

        if (!m_loadingScene)
        {
            frameContext.renderList = SceneManager::GetInstance().renderList;
            frameContext.buildUI = [this]()
            {
                BuildUI();
            };
            OnUpdate();
            BufferManager::GetInstance().Update(frameContext);
        }

        frameContext.pCamera = CameraManager::GetInstance().GetMainCamera();
        m_pRenderer->OnRender(frameContext);

        m_pDevice->SubmitContextWork(*m_pGraphicsContext);

        EndFrame();
        Present();
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
        auto fnIsKeyTriggered = [&io](char key)
        {
            return io.KeysDown[key] && io.KeysDownDuration[key] == 0.0f;
        };

        // Handle Keyboard/Mouse input here

        /* MAGNIFIER CONTROLS */
        if (fnIsKeyTriggered('L'))
            m_UIState.ToggleMagnifierLock();
        if (fnIsKeyTriggered('M') || io.MouseClicked[2])
            ToggleBool(m_UIState.bUseMagnifier); // middle mouse / M key toggles magnifier

        if (io.MouseClicked[1] && m_UIState.bUseMagnifier) // right mouse click
            m_UIState.ToggleMagnifierLock();

        if (fnIsKeyTriggered('R'))
            m_UIState.ResetLPMSceneDefaults();
    }

    void ElysiaFrame::BuildUI()
    {
        auto& pUserData = UserData::GetInstance();

        if (ImGui::CollapsingHeader("Debug"))
        {
            int debugModeIndex = (int)pUserData.debugMode;
            ImGui::Combo("Debug Mode", &debugModeIndex,
                         StringViewToChar(magic_enum::enum_names<DebugMode>().data(),
                                          magic_enum::enum_count<DebugMode>()).data(),
                         (int)magic_enum::enum_count<DebugMode>());
            debugModeIndex = std::clamp(debugModeIndex, 0,
                                        static_cast<int>(magic_enum::enum_count<DebugMode>()));
            pUserData.debugMode = (DebugMode)debugModeIndex;

            ImGui::SliderInt("mipmap level", &pUserData.mipmapLevel, 0, 10, "%.3f",
                             ImGuiSliderFlags_AlwaysClamp);
        }

        if (ImGui::CollapsingHeader("Light"))
        {
            ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
            ImGui::DragFloat3("Direction", (float*)&pUserData.lightDir, 1, -1, 1);
            ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 20, "%.3f");

            int shadowTypeIndex = (int)pUserData.shadowType;
            ImGui::Combo("Shadow Type", &shadowTypeIndex,
                         StringViewToChar(magic_enum::enum_names<ShadowType>().data(),
                                          magic_enum::enum_count<ShadowType>()).data(),
                         (int)magic_enum::enum_count<ShadowType>());
            shadowTypeIndex = std::clamp(shadowTypeIndex, 0,
                                         static_cast<int>(magic_enum::enum_count<ShadowType>()));
            pUserData.shadowType = (ShadowType)shadowTypeIndex;

            int shadowQualityIndex = (int)pUserData.shadowQuality;
            ImGui::Combo("Shadow Quality", &shadowQualityIndex,
                         StringViewToChar(magic_enum::enum_names<ShadowQuality>().data(),
                                          magic_enum::enum_count<ShadowQuality>()).data(),
                         (int)magic_enum::enum_count<ShadowQuality>());
            shadowQualityIndex = std::clamp(shadowQualityIndex, 0,
                                            static_cast<int>(magic_enum::enum_count<
                                                ShadowQuality>()));
            pUserData.shadowQuality = (ShadowQuality)shadowQualityIndex;

            ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowDepthBias, 0, 10);
            ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowSlopeDepthBias, 0, 10);
            ImGui::SliderFloat("Shadow Max Slope Depth Bias", &pUserData.shadowMaxSlopeDepthBias, 0,
                               10);
        }

        if (ImGui::CollapsingHeader("PBR Data"))
        {
            ImGui::ColorEdit3("Base Color Tint", (float*)&pUserData.BaseColorTint,
                              ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                              ImGuiColorEditFlags_HDR);
            ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f);
            ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f);
            ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Ambient Cubemap Intensity", &pUserData.AmbientCubemapIntensity, 0.f,
                               2.f);
            ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&pUserData.AmbientCubemapTint);
        }

        if (ImGui::CollapsingHeader("HDR"))
        {
            ImGui::Checkbox("Is Enable HDR", &pUserData.IsUseHDR);

            int HDRQualityIndex = (int)pUserData.HDRLevel;
            ImGui::Combo("HDR Quality", &HDRQualityIndex,
                         StringViewToChar(magic_enum::enum_names<HDRQuality>().data(),
                                          magic_enum::enum_count<HDRQuality>()).data(),
                         (int)magic_enum::enum_count<HDRQuality>());
            HDRQualityIndex = std::clamp(HDRQualityIndex, 0,
                                         static_cast<int>(magic_enum::enum_count<HDRQuality>()));
            pUserData.HDRLevel = (HDRQuality)HDRQualityIndex;

            int tonemapModeIndex = (int)pUserData.tonemapMode;
            ImGui::Combo("Tonemap Mode", &tonemapModeIndex,
                         StringViewToChar(magic_enum::enum_names<TonemapMode>().data(),
                                          magic_enum::enum_count<TonemapMode>()).data(),
                         (int)magic_enum::enum_count<TonemapMode>());
            tonemapModeIndex = std::clamp(tonemapModeIndex, 0,
                                          static_cast<int>(magic_enum::enum_count<TonemapMode>()));
            pUserData.tonemapMode = (TonemapMode)tonemapModeIndex;

            const char** displayModeNames = &m_displayModesNamesAvailable[0];
            if (ImGui::Combo("Display Mode", (int*)&m_currentDisplayModeNamesIndex,
                             displayModeNames, (int)m_displayModesNamesAvailable.size()))
            {
                if (m_fullscreenMode != PRESENTATIONMODE_WINDOWED)
                {
                    UpdateDisplay(m_displayModesAvailable[m_currentDisplayModeNamesIndex],
                                  m_disableLocalDimming);
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                }
                else if (CheckIfWindowModeHdrOn() &&
                         (m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_SDR
                          ||
                          m_displayModesAvailable[m_currentDisplayModeNamesIndex] ==
                          DISPLAYMODE_HDR10_2084 ||
                          m_displayModesAvailable[m_currentDisplayModeNamesIndex] ==
                          DISPLAYMODE_HDR10_SCRGB))
                {
                    UpdateDisplay(m_displayModesAvailable[m_currentDisplayModeNamesIndex],
                                  m_disableLocalDimming);
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                }
                else
                {
                    m_currentDisplayModeNamesIndex = m_previousDisplayModeNamesIndex;
                }

                UserData::GetInstance().displayMode = m_currentDisplayModeNamesIndex;
            }

            int colorSpaceIndex = (int)pUserData.colorSpace;
            ImGui::Combo("Color space", &colorSpaceIndex,
                         StringViewToChar(magic_enum::enum_names<ColorSpace>().data(),
                                          magic_enum::enum_count<ColorSpace>()).data(),
                         (int)magic_enum::enum_count<ColorSpace>());
            colorSpaceIndex = std::clamp(colorSpaceIndex, 0,
                                         static_cast<int>(magic_enum::enum_count<ColorSpace>()));
            pUserData.colorSpace = (ColorSpace)colorSpaceIndex;

            ImGui::Checkbox("Shoulder", &pUserData.bShoulder);
            ImGui::SliderFloat("Soft Gap", &pUserData.SoftGap, 0.0f, 0.5f);
            ImGui::SliderFloat("HDR Max", &pUserData.HdrMax, 8.0f, 2048.0f);
            ImGui::SliderFloat("LPM Exposure", &pUserData.LpmExposure, 3.0f, 11.0f);
            ImGui::SliderFloat("Contrast", &pUserData.Contrast, 0.0f, 1.0f);
            ImGui::SliderFloat("Shoulder Contrast", &pUserData.ShoulderContrast, 1.0f, 1.2f);
            ImGui::SliderFloat3("Saturation", &pUserData.Saturation[0], 0.0f, 2.0f);
            ImGui::SliderFloat3("Crosstalk", &pUserData.Crosstalk[0], 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("AO"))
        {

            ImGui::Checkbox("Is Enable AO", &pUserData.aoParameter.IsEnableAO);
            ImGui::Checkbox("Is IsLerp AO", &pUserData.aoParameter.IsLerpAO);

            ImGui::SliderInt("AO Sample Count", &pUserData.aoParameter.SampleCount, 0, 32);
            ImGui::SliderInt("AO Sample Step Count", &pUserData.aoParameter.SampleStepCount, 0, 6);

            ImGui::SliderFloat("AO Radius", &pUserData.aoParameter.Radius, 0.1, 50);
            ImGui::SliderFloat("AO Fade Radius", &pUserData.aoParameter.FadeRadius, 1, 20000);
            ImGui::SliderFloat("AO Fade Distance", &pUserData.aoParameter.FadeDistance, 1, 20000);

            ImGui::SliderFloat("AO Intensity", &pUserData.aoParameter.IntensityMul, 0, 1);

            ImGui::SliderFloat("AO Pow", &pUserData.aoParameter.IntensityPow, 0.1, 8);

            ImGui::SliderFloat("AO Bias", &pUserData.aoParameter.Bias, 0.f, 0.01f);

            ImGui::SliderFloat("AO Lerp", &pUserData.aoParameter.AOLerpFactor, 0.1f, 1.f);
            ImGui::SliderFloat("AO TAA Lerp Weight", &pUserData.aoParameter.TAALerpFactor, 0.05f, 0.1f);

            int blurQualityIndex = (int)pUserData.aoParameter.BlurQuality;
            ImGui::Combo("Blur Quality", &blurQualityIndex,
                         StringViewToChar(magic_enum::enum_names<AOBlurQuality>().data(),
                                          magic_enum::enum_count<AOBlurQuality>()).data(),
                         (int)magic_enum::enum_count<AOBlurQuality>());
            blurQualityIndex = std::clamp(blurQualityIndex, 0,
                                          static_cast<int>(magic_enum::enum_count<AOBlurQuality>()));
            pUserData.aoParameter.BlurQuality = (AOBlurQuality)blurQualityIndex;

            ImGui::SliderFloat("AO Sharpness", &pUserData.aoParameter.Sharpness, 1.f, 100.f);
        }
    }
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
    std::wstring name(L"Elysia Engine");

    return RunFramework(hInstance, lpCmdLine, nCmdShow, new ElysiaEngine::ElysiaFrame(name));
}