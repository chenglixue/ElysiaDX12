#include "stdafx.h"
#include "ElysiaFrame.h"


#include "ImGuiUtility.h"
#include "Editor/IMGUIDrawer.h"
#include "Editor/IMGUIHelper.h"
#include "Editor/UserData.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12StagingDescriptorHeap.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/LightManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/TextureManager.h"
#include "Runtime/Resource/Model/ModelManager.h"
#include "ECS/Entity.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Pass/GIPass.h"
#include "ThirdParty/imgui/imgui_internal.h"

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;

    static void ToggleBool(bool& b)
    {
        b = !b;
    }

    ElysiaFrame::ElysiaFrame(std::wstring name)
        : FrameworkWindows(name)
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
        // *pWidth = 1920;
        // *pHeight = 1080;

        m_VsyncEnabled = false;
        m_bIsBenchmarking = false;
        m_isCpuValidationLayerEnabled = false;
        m_isGpuValidationLayerEnabled = false;
        m_stablePowerState = false;

    }

    void ElysiaFrame::OnCreate()
    {
#ifdef _DEBUG
        assert(_CrtCheckMemory());
#endif
        DeSerializeUserData();
#ifdef _DEBUG
        assert(_CrtCheckMemory());
#endif

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
        m_pRenderer->OnCreate(m_pDevice,
                              &m_swapChain,
                              m_pGraphicsContext.get());

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
            CameraManager::GetInstance().CreateMainCamera(
                SceneManager::GetInstance().GetEntities().empty()
                    ? Vector3(-0.48, 5.2f, -0.31)
                    : SceneManager::GetInstance().GetEntities()[0]->GetWorldAABB().Center,
                static_cast<float>(m_Width) / static_cast<float>(m_Height),
                AMD_PI_OVER_4,
                0.1f,
                1000.f);

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
        auto frameContext = BeginFrame();
        m_pGraphicsContext->Reset();
        // ImGUI_UpdateIO();
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
            static bool firstInit = true;
            if (firstInit)
            {
                firstInit = false;
                BuildUI();
            }
            else
            {
                frameContext.buildUI = [this]()
                {
                    BuildUI();
                };
            }

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

        HandleInput(io);
        // CameraManager::GetInstance().GetMainCamera()->UpdateFrustum();
        // SceneManager::GetInstance().CollectRenderItems();
    }

    void ElysiaFrame::HandleInput(const ImGuiIO& io)
    {
        auto pCamera = CameraManager::GetInstance().GetMainCamera();
        if (!pCamera)
            return;
        auto pFirstPersonCam = dynamic_cast<FirstPersonCamera*>(pCamera);
        if (!pFirstPersonCam)
            return;

        const float sensitivity = 0.002f;
        const float moveSpeed = 2.f;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            Vector3 moveDir = Vector3::Zero;
            if (ImGui::IsKeyDown(ImGuiKey_W))
                moveDir.z += 1.0f; // 前
            if (ImGui::IsKeyDown(ImGuiKey_S))
                moveDir.z -= 1.0f; // 后
            if (ImGui::IsKeyDown(ImGuiKey_A))
                moveDir.x -= 1.0f; // 左
            if (ImGui::IsKeyDown(ImGuiKey_D))
                moveDir.x += 1.0f; // 右
            if (ImGui::IsKeyDown(ImGuiKey_E))
                moveDir.y += 1.0f; // 上 (可选)
            if (ImGui::IsKeyDown(ImGuiKey_Q))
                moveDir.y -= 1.0f; // 下 (可选)
            if (moveDir != Vector3::Zero)
            {
                moveDir.Normalize();
                pFirstPersonCam->Move(moveDir, io.DeltaTime);
            }

            float x = pFirstPersonCam->GetXRotation();
            float y = pFirstPersonCam->GetYRotation();
            x += io.MouseDelta.y * sensitivity;
            y += io.MouseDelta.x * sensitivity;
            pFirstPersonCam->SetXRotation(x);
            pFirstPersonCam->SetYRotation(y);

            if (m_pSelectedObject && m_pSelectedObject->pAttachedCamera == pFirstPersonCam)
            {
                m_pSelectedObject->transform.rotation = pFirstPersonCam->m_transform.rotation;
                m_pSelectedObject->transform.position = pFirstPersonCam->m_transform.position;
            }
        }

    }

    void ElysiaFrame::BuildUI()
    {
        SetupDockSpace();
        BuildUISceneHierarchy();
        BuildUIViewport();
        BuildUIInspector();
        BuildUIRenderSetting();
    }
    void ElysiaFrame::SetupDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        // 窗口始终完美覆盖主渲染窗口
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        // 样式设置：无边框、无标题栏、不可移动
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        // window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::Begin("MainDockHost", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        static bool layout_initialized = true;
        if (layout_initialized)
        {
            layout_initialized = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_id_left;
            ImGuiID dock_id_center;
            ImGuiID dock_id_right;

            ImGui::DockBuilderSplitNode(dockspace_id,
                                        ImGuiDir_Left,
                                        0.05f,
                                        &dock_id_left,
                                        &dock_id_center);

            ImGui::DockBuilderSplitNode(dock_id_center,
                                        ImGuiDir_Right,
                                        0.1f,
                                        &dock_id_right,
                                        &dock_id_center);

            ImGuiID dock_id_inspector;
            ImGuiID dock_id_render_settings;
            ImGui::DockBuilderSplitNode(dock_id_right,
                                        ImGuiDir_Up,
                                        0.50f,
                                        &dock_id_inspector,
                                        &dock_id_render_settings);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Render Settings", dock_id_render_settings);
            ImGui::DockBuilderDockWindow("Viewport", dock_id_center);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_inspector);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
    void ElysiaFrame::BuildUISceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !
            ImGui::IsAnyItemHovered())
        {
            m_pSelectedObject = nullptr;
        }

        auto& entities = SceneManager::GetInstance().GetEntities();

        for (auto& objPtr : entities)
        {
            Entity* obj = objPtr.get();
            DrawEntityNode(obj);
        }

        ImGui::End();
    }
    void ElysiaFrame::BuildUIViewport()
    {
        ImGui::Begin("Viewport",
                     nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        static ImVec2 lastSize = {0, 0};
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x != lastSize.x || viewportSize.y != lastSize.y)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // 这里可以画一个临时的占位符，或者让旧图拉伸显示
            }
            else
            {
                if (viewportSize.x > 0 && viewportSize.y > 0)
                {
                    m_pRenderer->OnCreateWindowSizeDependentResources(
                        &m_swapChain,
                        viewportSize.x,
                        viewportSize.y);

                    lastSize = viewportSize;
                }
            }

        }

        auto cameraRT = m_pRenderer->GetDisplayRT();
        if (cameraRT)
        {
            auto srcCPUHandle = cameraRT->GetTexture()->GetSRVDescriptor().GetCPUHandle();
            auto dstDescriptor = m_pDevice->GetImguiDescriptor();
            m_pDevice->GetDevice()->CopyDescriptorsSimple(
                1,
                dstDescriptor.GetCPUHandle(),
                srcCPUHandle,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
                );

            ImTextureID sceneTexID = (ImTextureID)dstDescriptor.GetGPUHandle().ptr;
            ImGui::Image(sceneTexID, viewportSize, ImVec2(0, 0), ImVec2(1, 1));
        }

        ImGui::End();
    }
    void ElysiaFrame::BuildUIInspector()
    {
        ImGui::Begin("Inspector");

        if (m_pSelectedObject == nullptr)
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "Select an object to view its properties.");
            ImGui::End();
            return;
        }

        // char nameBuffer[256];
        // const char* pName = m_pSelectedObject->name.c_str();
        // if (pName)
        // {
        //     strncpy_s(nameBuffer, m_pSelectedObject->name.c_str(), sizeof(nameBuffer));
        //     nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        // }
        // else
        // {
        //     strcpy_s(nameBuffer, "None"); // 如果没有选中物体，赋予默认值
        // }
        // if (ImGui::InputText("##ObjectName", nameBuffer, sizeof(nameBuffer)))
        // {
        //     m_pSelectedObject->SetName(nameBuffer);
        // }

        ImGui::Separator();

        DrawTransformComponent(m_pSelectedObject);

        ImGui::End();
    }
    void ElysiaFrame::BuildUIRenderSetting()
    {
        ImGui::Begin("Render Settings");
        auto& pUserData = UserData::GetInstance();

        if (ImGui::CollapsingHeader("Debug"))
        {
            int debugModeIndex = (int)pUserData.debugMode;
            ImGui::Combo("Debug Mode",
                         &debugModeIndex,
                         StringViewToChar(magic_enum::enum_names<DebugMode>().data(),
                                          magic_enum::enum_count<DebugMode>()).data(),
                         (int)magic_enum::enum_count<DebugMode>());
            debugModeIndex = std::clamp(debugModeIndex,
                                        0,
                                        static_cast<int>(magic_enum::enum_count<DebugMode>()));
            pUserData.debugMode = (DebugMode)debugModeIndex;

            if (pUserData.debugMode == DebugMode::AO)
            {
                int debugModeIndex = (int)pUserData.aoParameter.debugTarget;
                ImGui::Combo("AO Debug",
                             &debugModeIndex,
                             StringViewToChar(magic_enum::enum_names<AODebugTarget>().data(),
                                              magic_enum::enum_count<AODebugTarget>()).data(),
                             (int)magic_enum::enum_count<AODebugTarget>());
                debugModeIndex = std::clamp(debugModeIndex,
                                            0,
                                            static_cast<int>(magic_enum::enum_count<
                                                AODebugTarget>()));
                pUserData.aoParameter.debugTarget = (AODebugTarget)debugModeIndex;

                ImGui::SliderInt("mipmap level",
                                 &pUserData.mipmapLevel,
                                 0,
                                 10);
            }
            if (pUserData.debugMode == DebugMode::AABB || pUserData.debugMode == DebugMode::GIProbe)
            {
                ImGui::SliderInt("Instance GI",
                                 &pUserData.instanceID,
                                 0,
                                 400);
            }
            if (pUserData.debugMode == DebugMode::GIProbe)
            {
                ImGui::Checkbox("Enable Line", &pUserData.GIParameter.enableLine);
                ImGui::SliderFloat("Line Thicness", &pUserData.GIParameter.lineWidth, 0.f, 5.f);
                ImGui::Checkbox("Hide Inactive Probe", &pUserData.GIParameter.bHideInactiveProbe);
            }
            if (pUserData.debugMode == DebugMode::GI)
            {
                ImGui::Checkbox("Texture Visualization", &pUserData.GIParameter.bTextureVisualization);
                if (pUserData.GIParameter.bTextureVisualization)
                {
                    auto pIrradianceRT = GIPass::m_pIrradianceRT;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pIrradianceRT->GetTexture()->GetSRVDescriptor().
                                                                           GetGPUHandle();

                    // ImTextureID 本质上就是一个 void*，在 DX12 的 ImGui 实现中它就是 GPU 句柄的 ptr
                    ImTextureID texID = (ImTextureID)gpuHandle.ptr;

                    // 获取贴图的实际宽高，或者自定义一个显示尺寸
                    float width = pIrradianceRT->GetWidth();
                    // Atlas 的高宽比通常很极端 (比如 宽很长，高很短)，这里等比缩放
                    float height = pIrradianceRT->GetHeight();

                    // 绘制图片
                    ImGui::Image(texID, ImVec2(width, height));
                }
            }
            if (pUserData.debugMode == DebugMode::Bloom)
            {
                ElysiaRenderer::EnumCombo("Bloom Mode", &pUserData.bloomParameter.debugMode);
                ImGui::SliderInt("Bloom  Mipmap Level", &pUserData.bloomParameter.mipmap, 0, 5);

            }
        }

        if (ImGui::CollapsingHeader("Light"))
        {
            ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
            ImGui::SliderFloat3("Direction", (float*)&pUserData.lightDir, -1, 1);
            ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 20);

            ImGui::Checkbox("Enable Shadow", &pUserData.EnableShadow);
            int shadowTypeIndex = (int)pUserData.shadowType;
            ImGui::Combo("Shadow Type",
                         &shadowTypeIndex,
                         StringViewToChar(magic_enum::enum_names<ShadowType>().data(),
                                          magic_enum::enum_count<ShadowType>()).data(),
                         (int)magic_enum::enum_count<ShadowType>());
            shadowTypeIndex = std::clamp(shadowTypeIndex,
                                         0,
                                         static_cast<int>(magic_enum::enum_count<ShadowType>()));
            pUserData.shadowType = (ShadowType)shadowTypeIndex;

            int shadowQualityIndex = (int)pUserData.shadowQuality;
            ImGui::Combo("Shadow Quality",
                         &shadowQualityIndex,
                         StringViewToChar(magic_enum::enum_names<ShadowQuality>().data(),
                                          magic_enum::enum_count<ShadowQuality>()).data(),
                         (int)magic_enum::enum_count<ShadowQuality>());
            shadowQualityIndex = std::clamp(shadowQualityIndex,
                                            0,
                                            static_cast<int>(magic_enum::enum_count<
                                                ShadowQuality>()));
            pUserData.shadowQuality = (ShadowQuality)shadowQualityIndex;

            ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowDepthBias, 0, 1);
            ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowSlopeDepthBias, 0, 10);
            ImGui::SliderFloat("Shadow Max Slope Depth Bias",
                               &pUserData.shadowMaxSlopeDepthBias,
                               0,
                               10);
        }

        if (ImGui::CollapsingHeader("PBR Data"))
        {
            ImGui::ColorEdit3("Base Color Tint",
                              (float*)&pUserData.BaseColorTint,
                              ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                              ImGuiColorEditFlags_HDR);
            ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f);
            ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f);
            ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 5.f);
            ImGui::SliderFloat("Ambient Cubemap Intensity",
                               &pUserData.AmbientCubemapIntensity,
                               0.f,
                               20.f);
            ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&pUserData.AmbientCubemapTint);
            ImGui::SliderFloat("GI Normal Bias", (float*)&pUserData.GIParameter.normalBias, 0, 0.5);
            ImGui::SliderFloat("GI View Bias", (float*)&pUserData.GIParameter.viewBias, 0, 2);
            ImGui::SliderFloat("GI Blend Weight",
                               (float*)&pUserData.GIParameter.blendWeight,
                               0.9,
                               0.99);
            ImGui::SliderFloat("GI Encoding Gamma",
                               (float*)&pUserData.GIParameter.gamma,
                               1.f,
                               10.f);
            ImGui::SliderFloat("GI Irradiance Threshold",
                               (float*)&pUserData.GIParameter.probeIrradianceThreshold,
                               0.001f,
                               1.0f);
            ImGui::SliderFloat("GI Brightness Threshold",
                               (float*)&pUserData.GIParameter.probeBrightnessThreshold,
                               1.f,
                               5.f);
            ImGui::DragFloat3("GI Probe Group Origin",
                              (float*)&pUserData.GIParameter.probeGroupOrigin,
                              0.1f);
        }

        if (ImGui::CollapsingHeader("Postprocess"))
        {
            ImGui::Indent();
            if (ImGui::CollapsingHeader("HDR"))
            {
                ImGui::Checkbox("Is Enable HDR", &pUserData.IsUseHDR);

                int HDRQualityIndex = (int)pUserData.HDRLevel;
                ImGui::Combo("HDR Quality",
                             &HDRQualityIndex,
                             StringViewToChar(magic_enum::enum_names<HDRQuality>().data(),
                                              magic_enum::enum_count<HDRQuality>()).data(),
                             (int)magic_enum::enum_count<HDRQuality>());
                HDRQualityIndex = std::clamp(HDRQualityIndex,
                                             0,
                                             static_cast<int>(magic_enum::enum_count<HDRQuality>()));
                pUserData.HDRLevel = (HDRQuality)HDRQualityIndex;

                int tonemapModeIndex = (int)pUserData.tonemapMode;
                ImGui::Combo("Tonemap Mode",
                             &tonemapModeIndex,
                             StringViewToChar(magic_enum::enum_names<TonemapMode>().data(),
                                              magic_enum::enum_count<TonemapMode>()).data(),
                             (int)magic_enum::enum_count<TonemapMode>());
                tonemapModeIndex = std::clamp(tonemapModeIndex,
                                              0,
                                              static_cast<int>(magic_enum::enum_count<TonemapMode>()));
                pUserData.tonemapMode = (TonemapMode)tonemapModeIndex;

                const char** displayModeNames = &m_displayModesNamesAvailable[0];
                if (ImGui::Combo("Display Mode",
                                 (int*)&m_currentDisplayModeNamesIndex,
                                 displayModeNames,
                                 (int)m_displayModesNamesAvailable.size()))
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
                ImGui::Combo("Color space",
                             &colorSpaceIndex,
                             StringViewToChar(magic_enum::enum_names<ColorSpace>().data(),
                                              magic_enum::enum_count<ColorSpace>()).data(),
                             (int)magic_enum::enum_count<ColorSpace>());
                colorSpaceIndex = std::clamp(colorSpaceIndex,
                                             0,
                                             static_cast<int>(magic_enum::enum_count<ColorSpace>()));
                pUserData.colorSpace = (ColorSpace)colorSpaceIndex;

                ImGui::Checkbox("Shoulder", &pUserData.bShoulder);
                ImGui::SliderFloat("Local Exposure", &pUserData.localExposure, 0.0f, 5.f);
                ImGui::SliderFloat("Soft Gap", &pUserData.SoftGap, 0.0f, 0.5f);
                ImGui::SliderFloat("HDR Max", &pUserData.HdrMax, 8.0f, 2048.0f);
                ImGui::SliderFloat("LPM Exposure", &pUserData.LpmExposure, 3.0f, 11.0f);
                ImGui::SliderFloat("Contrast", &pUserData.Contrast, 0.0f, 1.0f);
                ImGui::SliderFloat("Shoulder Contrast", &pUserData.ShoulderContrast, 1.0f, 1.2f);
                ImGui::SliderFloat3("Saturation", (float*)&pUserData.Saturation, 0.0f, 2.0f);
                ImGui::SliderFloat3("Crosstalk", (float*)&pUserData.Crosstalk, 0.0f, 1.0f);
            }

            if (ImGui::CollapsingHeader("AO"))
            {
                ImGui::Checkbox("Is Enable AO", &pUserData.aoParameter.IsEnableAO);
                ImGui::Checkbox("Is IsLerp AO", &pUserData.aoParameter.IsLerpAO);
                ImGui::Checkbox("Is Blur", &pUserData.aoParameter.IsBlur);

                ImGui::SliderFloat("AO Radius", &pUserData.aoParameter.Radius, 0.1, 2);
                ImGui::SliderFloat("AO Fade Radius", &pUserData.aoParameter.FadeRadius, 1, 20000);
                ImGui::SliderFloat("AO Fade Distance", &pUserData.aoParameter.FadeDistance, 1, 20000);

                ImGui::SliderFloat("AO Intensity", &pUserData.aoParameter.IntensityMul, 0, 2);

                ImGui::SliderFloat("AO Pow", &pUserData.aoParameter.IntensityPow, 0.1, 8);

                ImGui::SliderFloat("AO Bias", &pUserData.aoParameter.Bias, 0.f, 0.01f);
                ImGui::SliderFloat("AO HIZ Mip Factor", &pUserData.aoParameter.HIZMipFactor, 0.f, 1.f);

                ImGui::SliderFloat("AO TAA Lerp Weight",
                                   &pUserData.aoParameter.TAALerpFactor,
                                   0.05f,
                                   0.1f);

                int blurQualityIndex = (int)pUserData.aoParameter.BlurQuality;
                ImGui::Combo("Blur Quality",
                             &blurQualityIndex,
                             StringViewToChar(magic_enum::enum_names<AOBlurQuality>().data(),
                                              magic_enum::enum_count<AOBlurQuality>()).data(),
                             (int)magic_enum::enum_count<AOBlurQuality>());
                blurQualityIndex = std::clamp(blurQualityIndex,
                                              0,
                                              static_cast<int>(magic_enum::enum_count<
                                                  AOBlurQuality>()));
                pUserData.aoParameter.BlurQuality = (AOBlurQuality)blurQualityIndex;

                ImGui::SliderInt("AO Blur Count", &pUserData.aoParameter.BlurCount, 1, 4);
                ImGui::SliderInt("AO Blur Radius", &pUserData.aoParameter.BlurIntensity, 1, 10);
                ImGui::SliderFloat("AO Sharpness", &pUserData.aoParameter.Sharpness, 0.f, 1.f);

                ImGui::Checkbox("Is Enable TAA", &pUserData.aoParameter.IsTAA);
            }

            if (ImGui::CollapsingHeader("Bloom"))
            {
                ImGui::Checkbox("Enable Bloom ", &pUserData.bloomParameter.enable);
                ImGui::SliderFloat("Bloom Radius", &pUserData.bloomParameter.radius, 0.f, 10.f);
                ImGui::SliderFloat("Bloom Intensity", &pUserData.bloomParameter.intensity, 0.f, 5.f);
            }

            if (ImGui::CollapsingHeader("TAA"))
            {
                ImGui::Checkbox("Enable TAA", &pUserData.taaParameter.Enable);
                ImGui::SliderFloat("Sample Ratio", &pUserData.taaParameter.sampleRate, 0.5f, 1.f);
                ElysiaRenderer::EnumCombo("TAA Jitter Type", &pUserData.taaParameter.jitterType);

                ImGui::SliderFloat("TAA Jitter Intensity", &pUserData.taaParameter.jitterIntensity, 0.f, 2.f);
                ImGui::SliderFloat("TAA Static Weight", &pUserData.taaParameter.staticWeight, 0.9f, 1.f);
                ImGui::SliderFloat("TAA Dynamic Weight", &pUserData.taaParameter.dynamicWeight, 0.f, 0.3f);
                ImGui::SliderFloat("TAA Max Weight", &pUserData.taaParameter.maxWeight, 0.1f, 1.f);
            }

            if (ImGui::CollapsingHeader("Sharpen"))
            {
                ImGui::Checkbox("Enable Sharpen", &pUserData.sharpenParameter.enable);
                ImGui::SliderFloat("Shapren Intensity", &pUserData.sharpenParameter.sharpen, 0.f, 2.f);
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Timing"))
        {
            for (const auto& ts : m_pRenderer->GetTimingValues())
            {
                ImGui::Text("%s: %.2f us", ts.m_label.c_str(), ts.m_microseconds);
            }
        }
        ImGui::End();
    }

    void ElysiaFrame::DrawEntityNode(Entity* entity)
    {
        if (!entity)
            return;

        // 1. 准备节点标志
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        // 如果被选中，加上高亮标志
        if (m_pSelectedObject == entity)
            flags |= ImGuiTreeNodeFlags_Selected;

        // 如果没有子节点，标记为叶子节点（不显示箭头）
        bool hasChildren = !entity->GetChildren().empty();
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        // 2. 渲染节点
        // 使用指针作为唯一 ID，节点显示名称
        bool opened = ImGui::TreeNodeEx((void*)entity, flags, entity->name.c_str());

        // 3. 处理点击交互
        if (ImGui::IsItemClicked())
        {
            m_pSelectedObject = entity;
        }

        // 4. 如果节点被展开，递归绘制子节点
        if (opened)
        {
            for (auto& child : entity->GetChildren()) // 假设 children 存储的是原始指针或 smart ptr
            {
                DrawEntityNode(child.get());
            }
            ImGui::TreePop(); // 必须与打开的节点配对
        }
    }
    void ElysiaFrame::DrawTransformComponent(Entity* entity)
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;
            auto& transform = entity->transform;

            if (ImGui::DragFloat3("Position", (float*)&transform.position, 0.1f))
            {
                changed = true;
            }

            Vector3 currentEuler = transform.GetEulerDegrees();
            if (ImGui::SliderFloat3("Rotation", (float*)&currentEuler, -180.f, 180.f))
            {
                float p = XMConvertToRadians(currentEuler.x);
                float y = XMConvertToRadians(currentEuler.y);
                float r = XMConvertToRadians(currentEuler.z);
                transform.rotation = Quaternion::CreateFromYawPitchRoll(y, p, r);

                changed = true;
            }
            // }// if (ImGui::DragFloat3("Rotation", (float*)&transform.rotation))
            //            // {
            //            //     changed = true;
            //            // }

            if (ImGui::DragFloat3("Scale", (float*)&transform.scale, 0.1f))
            {
                changed = true;
            }

            if (changed)
            {
                entity->OnTransformChanged();
            }
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