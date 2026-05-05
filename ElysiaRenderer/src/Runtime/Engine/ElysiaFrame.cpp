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
#include "Runtime/RenderCore/BakeManager.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/RenderPassResourceManager.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Pass/GBufferPass.h"
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

        BakeManager::GetInstance().Init(m_pDevice);
        BufferManager::GetInstance().Init(m_pDevice);
        TextureManager::GetInstance().Init(m_pDevice);
        RenderTargetManager::GetInstance().Init(m_pDevice);
        CameraManager::GetInstance().Init(m_pDevice);
        LightManager::GetInstance().Init(m_pDevice);
        PSOManager::GetInstance().Init(m_pDevice);
        SceneManager::GetInstance().Init(m_pDevice);
        RenderPassResourceManager::GetInstance().Init(m_pDevice);

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
        CameraManager::GetInstance().GetMainCamera()->UpdateFrustum();
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
        BuildMainMenuBar();
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
    void ElysiaFrame::BuildMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Bake"))
            {
                if (ImGui::MenuItem("Pre-integrate SSS LUT"))
                {
                    BakeManager::GetInstance().RequestMasks(EBakeTaskFlags::SSSLut);
                }

                if (ImGui::MenuItem("Pre-integrate SSS NDF LUT"))
                {
                    BakeManager::GetInstance().RequestMasks(EBakeTaskFlags::SSSNDFLut);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Bake All Pre-computations"))
                {
                    BakeManager::GetInstance().RequestMasks(EBakeTaskFlags::All);
                }

                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();
    }
    void ElysiaFrame::BuildUIRenderSetting()
    {
        ImGui::Begin("Render Settings");
        auto& pUserData = UserData::GetInstance();

        ImGui::Checkbox("Enable HIZ", &pUserData.EnableHIZ);
        ImGui::Text("GBuffer Render Count: %u", GBufferPass::m_renderCount);
        if (ImGui::CollapsingHeader("Debug"))
        {
            ElysiaRenderer::EnumCombo("Debug Mode", &pUserData.debugMode);

            if (pUserData.debugMode == DebugMode::AO)
            {
                ElysiaRenderer::EnumCombo("AO Debug", &pUserData.aoParameter.debugTarget);
                ImGui::SliderInt("mipmap level",
                                 &pUserData.mipmapLevel,
                                 0,
                                 5);
            }
            if (pUserData.debugMode == DebugMode::AABB || pUserData.debugMode == DebugMode::GIProbe)
            {
                ImGui::SliderInt("Instance GI",
                                 &pUserData.instanceID,
                                 0,
                                 102);
            }
            if (pUserData.debugMode == DebugMode::GIProbe)
            {
                ImGui::Checkbox("Enable Line", &pUserData.GIParameter.enableLine);
                ImGui::SliderFloat("Line Thicness", &pUserData.GIParameter.lineWidth, 0.f, 5.f);
                ImGui::Checkbox("Hide Inactive Probe", &pUserData.GIParameter.bHideInactiveProbe);
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

            ImGui::Checkbox("Enable Shadow", &pUserData.shadowParameter.EnableShadow);
            if (ElysiaRenderer::EnumCombo("Shadow Type", &pUserData.shadowParameter.shadowType))
            {
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
            }
            if (ElysiaRenderer::EnumCombo("Shadow Quality", &pUserData.shadowParameter.shadowQuality))
            {
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
            }
            ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowParameter.shadowDepthBias, 0, 1);
            ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowParameter.shadowSlopeDepthBias, 0, 10);
            ImGui::SliderFloat("Shadow Max Slope Depth Bias",
                               &pUserData.shadowParameter.shadowMaxSlopeDepthBias,
                               0,
                               10);
            ImGui::SliderFloat("Shadow Radius",
                               &pUserData.shadowParameter.shadowRadius,
                               0,
                               5);
            ImGui::Checkbox("Enable Shadow TAA",
                            &pUserData.shadowParameter.EnableTAA);
        }

        if (ImGui::CollapsingHeader("PBR Data"))
        {
            if (ElysiaRenderer::EnumCombo("Shading Model", &pUserData.shadingModelID))
            {
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
            }
            if (pUserData.shadingModelID == ShadingModel::Preintegrated_Skin)
            {
                ImGui::SliderFloat("Curve Scale", &pUserData.subsurfaceScatterParameter.CurveScale, 0.f, 2.f);
                ImGui::SliderFloat("Min Curve", &pUserData.subsurfaceScatterParameter.MinCurve, 0.f, 1.f);
                ImGui::ColorEdit3("Subsurface Color",
                                  (float*)&pUserData.subsurfaceScatterParameter.SubsurfaceColor,
                                  ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                                  ImGuiColorEditFlags_HDR);
                ImGui::SliderFloat("Scatter Radius", &pUserData.subsurfaceScatterParameter.ScatterRadius, 0.f, 2.f);
                ImGui::SliderFloat("Transmission Scale",
                                   &pUserData.subsurfaceScatterParameter.TransmissionScale,
                                   0.f,
                                   5.f);
                ImGui::SliderFloat("Transmission Range",
                                   &pUserData.subsurfaceScatterParameter.TransmissionRange,
                                   0.f,
                                   2.f);
                ImGui::SliderFloat("Transmission Edge Glow",
                                   &pUserData.subsurfaceScatterParameter.TransmissionEdgeGlow,
                                   0.f,
                                   1.f);
            }
            if (pUserData.shadingModelID == ShadingModel::Hair)
            {
                if (ImGui::Checkbox("Enable Multi Scatter", &pUserData.hairParameter.bEnableMultiScatter))
                {
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                }
                if (ImGui::Checkbox("Enable R", &pUserData.hairParameter.bEnableR))
                {
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                }
                if (ImGui::Checkbox("Enable TT", &pUserData.hairParameter.bEnableTT))
                {
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                }
                if (ImGui::Checkbox("Enable TRT", &pUserData.hairParameter.bEnableTRT))
                {
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                }

                ImGui::SliderFloat("Back Lit", &pUserData.hairParameter.backLit, 0.f, 1.f);
            }
            ImGui::ColorEdit3("Base Color Tint",
                              (float*)&pUserData.BaseColorTint,
                              ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                              ImGuiColorEditFlags_HDR);
            ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f);
            ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f);
            ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 2.f);
            ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 1.f);
            ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 1.f);
            ImGui::SliderFloat("Specular", &pUserData.Specular, 0.f, 1.f);
            ImGui::ColorEdit3("Emission Tint", (float*)&pUserData.EmissionTint);
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
                ImGui::Checkbox("Is Enable HDR", &pUserData.hdrParameter.IsUseHDR);
                ElysiaRenderer::EnumCombo("HDR Quality", &pUserData.hdrParameter.HDRLevel);
                ElysiaRenderer::EnumCombo("Tonemap Mode", &pUserData.hdrParameter.tonemapMode);

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

                    UserData::GetInstance().hdrParameter.displayMode = m_currentDisplayModeNamesIndex;
                }
                ElysiaRenderer::EnumCombo("Color space", &pUserData.hdrParameter.colorSpace);

                ImGui::Checkbox("Shoulder", &pUserData.hdrParameter.bShoulder);
                ImGui::SliderFloat("Local Exposure", &pUserData.hdrParameter.localExposure, 0.0f, 5.f);
                ImGui::SliderFloat("Soft Gap", &pUserData.hdrParameter.SoftGap, 0.0f, 0.5f);
                ImGui::SliderFloat("HDR Max", &pUserData.hdrParameter.HdrMax, 8.0f, 2048.0f);
                ImGui::SliderFloat("LPM Exposure", &pUserData.hdrParameter.LpmExposure, 3.0f, 11.0f);
                ImGui::SliderFloat("Contrast", &pUserData.hdrParameter.Contrast, 0.0f, 1.0f);
                ImGui::SliderFloat("Shoulder Contrast", &pUserData.hdrParameter.ShoulderContrast, 1.0f, 1.2f);
                ImGui::SliderFloat3("Saturation", (float*)&pUserData.hdrParameter.Saturation, 0.0f, 2.0f);
                ImGui::SliderFloat3("Crosstalk", (float*)&pUserData.hdrParameter.Crosstalk, 0.0f, 1.0f);
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
                ElysiaRenderer::EnumCombo("Blur Quality", &pUserData.aoParameter.BlurQuality);

                ImGui::SliderInt("AO Blur Count", &pUserData.aoParameter.BlurCount, 1, 4);
                ImGui::SliderInt("AO Blur Radius", &pUserData.aoParameter.BlurIntensity, 1, 10);
                ImGui::SliderFloat("AO Sharpness", &pUserData.aoParameter.Sharpness, 0.f, 1.f);

                ImGui::Checkbox("Is Enable TAA", &pUserData.aoParameter.IsTAA);
            }

            if (ImGui::CollapsingHeader("Bloom"))
            {
                ImGui::Checkbox("Enable Bloom ", &pUserData.bloomParameter.enable);
                ImGui::SliderFloat("Bloom Radius", &pUserData.bloomParameter.radius, 0.f, 2.f);
                ImGui::SliderFloat("Bloom Intensity", &pUserData.bloomParameter.intensity, 0.f, 3.f);
            }

            if (ImGui::CollapsingHeader("TAA"))
            {
                ImGui::Checkbox("Enable TAA", &pUserData.taaParameter.Enable);
                if (ImGui::SliderFloat("Sample Ratio", &pUserData.taaParameter.sampleRate, 0.5f, 1.f))
                {
                    m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
                }

                ElysiaRenderer::EnumCombo("TAA Jitter Type", &pUserData.taaParameter.jitterType);

                ImGui::SliderFloat("TAA Jitter Intensity", &pUserData.taaParameter.jitterIntensity, 0.f, 2.f);
                ImGui::SliderFloat("TAA Static Weight", &pUserData.taaParameter.staticWeight, 0.9f, 1.f);
                ImGui::SliderFloat("TAA Dynamic Weight", &pUserData.taaParameter.dynamicWeight, 0.f, 0.3f);
                ImGui::SliderFloat("TAA Max Weight", &pUserData.taaParameter.maxWeight, 0.5f, 1.f);
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
            if (ImGui::BeginTable("TimingTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Pass / Category");
                ImGui::TableSetupColumn("Time (us)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                // 临时按前缀分组
                std::map<std::string, std::vector<TimeStamp>> categorizedTimes;
                for (const auto& ts : m_pRenderer->GetTimingValues())
                {
                    size_t pos = ts.m_label.find('/');
                    std::string category = (pos != std::string::npos) ? ts.m_label.substr(0, pos) : "Uncategorized";
                    std::string passName = (pos != std::string::npos) ? ts.m_label.substr(pos + 1) : ts.m_label;

                    categorizedTimes[category].push_back({passName, ts.m_microseconds}); // 伪代码构造
                }

                // 渲染分类树
                for (const auto& [category, passes] : categorizedTimes)
                {
                    float categoryTotalTime = 0.0f;
                    for (const auto& p : passes)
                        categoryTotalTime += p.m_microseconds;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    // 分类节点（粗体或不同颜色提示）
                    bool nodeOpen = ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", categoryTotalTime); // 显示该类别的总耗时

                    if (nodeOpen)
                    {
                        for (const auto& ts : passes)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("  %s", ts.m_label.c_str()); // 缩进表示层级

                            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                            if (ts.m_microseconds > 2000.0f)
                                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                            else if (ts.m_microseconds > 500.0f)
                                color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextColored(color, "%.2f", ts.m_microseconds);
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTable();
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

        if (ImGui::CollapsingHeader("Material Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {

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