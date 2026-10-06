#include "stdafx.h"
#include "../public/ElysiaFrame.h"


#include "../public/ImGuiUtility.h"
#include "Editor/public/IMGUIDrawer.h"
#include "Editor/public/IMGUIHelper.h"
#include "Editor/public/UserData.h"
#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/public/DX12StagingDescriptorHeap.h"
#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/LightManager.h"
#include "Runtime/RenderCore/public/DX12Shadow.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/CameraManager.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/SceneManager.h"
#include "Runtime/RenderCore/public/SelectionManager.h"
#include "Runtime/RenderCore/public/TextureManager.h"
#include "Runtime/Resource/Model/public/MaterialOverrides.h"
#include "Runtime/Resource/Model/public/ModelManager.h"
#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/RenderCore/public/MeshRenderer.h"
#include "Runtime/RenderCore/public/BakeManager.h"
#include "Runtime/RenderCore/public/DX12Camera.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/Pass/public/GBufferPass.h"
#include "Runtime/RenderCore/Pass/public/GIPass.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/ImGuizmo/ImGuizmo.h"
#include "Editor/public/OutputLogPanel.h"
#include "Programs/public/LogHistory.h"

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;

    static void ToggleBool(bool& b)
    {
        b = !b;
    }

    namespace
    {
        // ImGuizmo's internal matrix type uses the same convention as SimpleMath
        // (row-vector maths with the translation in the 4th row, i.e. m16[12..14]),
        // so the matrices are passed through unchanged. Transposing them would move
        // the translation out of m16[12..14] and ImGuizmo would read the object as
        // being at the origin.
        void ToGizmoMatrix(const Matrix& in, float out[16])
        {
            memcpy(out, &in, sizeof(float) * 16);
        }

        Matrix FromGizmoMatrix(const float in[16])
        {
            Matrix result;
            memcpy(&result, in, sizeof(float) * 16);
            return result;
        }

        constexpr size_t GizmoUndoStackLimit = 64;
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
        RestoreSavedDisplayMode();
        ElysiaHelper::LogHistory::ImportBuildDiagnostics();
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
        ModelManager::GetInstance().FlushMaterialEdits();
        m_pDevice->WaitForIdle();
        m_pGraphicsContext.release();
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnDestory();
        delete m_pRenderer;
        m_pRenderer = nullptr;
        PSOManager::GetInstance().Destory();
    }

    void ElysiaFrame::OnResize()
    {
        if (m_Width && m_Height && m_pRenderer)
        {
            Vector3 lookAt(-0.48f, 5.2f, -0.31f);
            const auto& sceneEntities = SceneManager::GetInstance().GetEntities();
            if (!sceneEntities.empty())
            {
                auto isFiniteBox = [](const BoundingBox& box)
                {
                    return std::isfinite(box.Center.x) && std::isfinite(box.Center.y) &&
                           std::isfinite(box.Center.z) && std::isfinite(box.Extents.x) &&
                           std::isfinite(box.Extents.y) && std::isfinite(box.Extents.z) &&
                           box.Extents.x >= 0.0f && box.Extents.y >= 0.0f && box.Extents.z >= 0.0f;
                };
                BoundingBox sceneAABB = sceneEntities[0]->GetWorldAABB();
                for (size_t entityIndex = 1; entityIndex < sceneEntities.size(); ++entityIndex)
                {
                    const BoundingBox nextBox = sceneEntities[entityIndex]->GetWorldAABB();
                    if (!isFiniteBox(nextBox))
                        continue;
                    if (!isFiniteBox(sceneAABB))
                    {
                        sceneAABB = nextBox;
                        continue;
                    }
                    const Vector3 dstMin = Vector3(sceneAABB.Center.x, sceneAABB.Center.y, sceneAABB.Center.z) -
                                           Vector3(sceneAABB.Extents.x, sceneAABB.Extents.y, sceneAABB.Extents.z);
                    const Vector3 dstMax = Vector3(sceneAABB.Center.x, sceneAABB.Center.y, sceneAABB.Center.z) +
                                           Vector3(sceneAABB.Extents.x, sceneAABB.Extents.y, sceneAABB.Extents.z);
                    const Vector3 srcMin = Vector3(nextBox.Center.x, nextBox.Center.y, nextBox.Center.z) -
                                           Vector3(nextBox.Extents.x, nextBox.Extents.y, nextBox.Extents.z);
                    const Vector3 srcMax = Vector3(nextBox.Center.x, nextBox.Center.y, nextBox.Center.z) +
                                           Vector3(nextBox.Extents.x, nextBox.Extents.y, nextBox.Extents.z);
                    const Vector3 mergedMin = Vector3::Min(dstMin, srcMin);
                    const Vector3 mergedMax = Vector3::Max(dstMax, srcMax);
                    const Vector3 mergedCenter = (mergedMin + mergedMax) * 0.5f;
                    const Vector3 mergedExtents = (mergedMax - mergedMin) * 0.5f;
                    sceneAABB.Center = mergedCenter;
                    sceneAABB.Extents = mergedExtents;
                }
                if (isFiniteBox(sceneAABB))
                    lookAt = Vector3(sceneAABB.Center.x, sceneAABB.Center.y, sceneAABB.Center.z);
            }
            CameraManager::GetInstance().CreateMainCamera(
                lookAt,
                static_cast<float>(m_Width) / static_cast<float>(m_Height),
                AMD_PI_OVER_4,
                0.1f,
                1000.f);

            m_pRenderer->OnDestroyWindowSizeDependentResources();
            m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
        }
    }

    void ElysiaFrame::RestoreSavedDisplayMode()
    {
        const CAULDRON_DX12::DisplayMode saved = UserData::GetInstance().hdrParameter.displayMode;
        int found = -1;
        for (int index = 0; index < static_cast<int>(m_displayModesAvailable.size()); ++index)
        {
            if (m_displayModesAvailable[index] == saved)
            {
                found = index;
                break;
            }
        }
        if (found < 0)
            return;

        m_currentDisplayModeNamesIndex = static_cast<CAULDRON_DX12::DisplayMode>(found);
        m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
        if (m_currentDisplayMode != saved)
            UpdateDisplay(saved, m_disableLocalDimming);
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

            if (auto* pSelectedObject = SelectionManager::GetInstance().GetSelected();
                pSelectedObject && pSelectedObject->pAttachedCamera == pFirstPersonCam)
            {
                pSelectedObject->transform.rotation = pFirstPersonCam->m_transform.rotation;
                pSelectedObject->transform.position = pFirstPersonCam->m_transform.position;
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
        if (m_UIState.bShowOutputLog)
            ElysiaEditor::DrawOutputLog(m_UIState.bShowOutputLog);
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

        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        {
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

            ImGuiID dock_id_viewport;
            ImGuiID dock_id_output;
            ImGui::DockBuilderSplitNode(dock_id_center,
                                        ImGuiDir_Down,
                                        0.22f,
                                        &dock_id_output,
                                        &dock_id_viewport);

            ImGuiID dock_id_inspector;
            ImGuiID dock_id_render_settings;
            ImGui::DockBuilderSplitNode(dock_id_right,
                                        ImGuiDir_Up,
                                        0.50f,
                                        &dock_id_inspector,
                                        &dock_id_render_settings);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Render Settings", dock_id_render_settings);
            ImGui::DockBuilderDockWindow("Viewport", dock_id_viewport);
            ImGui::DockBuilderDockWindow("Output Log", dock_id_output);
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
            SelectionManager::GetInstance().Clear();
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
            const ImVec2 imageOrigin = ImGui::GetItemRectMin();

            // Viewport click picking: LMB click on the image selects the entity,
            // syncing Scene Hierarchy and Inspector. While the transform gizmo is
            // hovered or being dragged it owns the mouse, so picking stands down.
            if (viewportSize.x > 0 && viewportSize.y > 0 &&
                ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
            {
                const ImVec2 mousePos = ImGui::GetIO().MousePos;
                const Vector2 viewportUV((mousePos.x - imageOrigin.x) / viewportSize.x,
                                         (mousePos.y - imageOrigin.y) / viewportSize.y);

                SelectionManager::GetInstance().RequestPick(viewportUV);
            }

            DrawViewportGizmo(imageOrigin, viewportSize);

            if (m_bShowShadowFrustum)
            {
                DrawShadowFrustumOverlay(imageOrigin, viewportSize);
            }
        }

        ImGui::End();
    }
    void ElysiaFrame::BuildUIInspector()
    {
        ImGui::Begin("Inspector");

        Entity* pSelectedObject = SelectionManager::GetInstance().GetSelected();
        if (pSelectedObject == nullptr)
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

        DrawTransformComponent(pSelectedObject);

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

            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("Output Log", nullptr, &m_UIState.bShowOutputLog);
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
            ImGui::Checkbox("Show Shadow Frustum", &m_bShowShadowFrustum);

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
            // UE parity: UDirectionalLightComponent::LightSourceAngle (angular diameter of the
            // sun disc, default 0.5357 deg = the real sun). In UE this angle only becomes a
            // shadow filter radius inside the PCSS path (ShadowRendering.h: PCSSParameters.x =
            // tan(0.5 * angle) * SZ / SW); the non-PCSS path (ShadowFilteringCommon.ush
            // ManualPCF) uses fixed 1x1/3x3/5x5 kernels and ignores it. This renderer has no
            // PCSS, so the value is kept for parity/documentation only and does NOT change the
            // shadow softness - use "Shadow Radius" below for that.
            ImGui::SliderFloat("Light Source Angle (deg)",
                               &pUserData.lightSourceAngleDegrees,
                               0.0f,
                               5.0f,
                               "%.3f");

            ImGui::Checkbox("Enable Shadow", &pUserData.shadowParameter.EnableShadow);
            if (ElysiaRenderer::EnumCombo("Shadow Type", &pUserData.shadowParameter.shadowType))
            {
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
            }
            if (ElysiaRenderer::EnumCombo("Shadow Quality", &pUserData.shadowParameter.shadowQuality))
            {
                // Only the shadow map resolution changes: rebuild that one texture
                // (plus the shadow keyword/PSO selection), instead of recreating
                // every window-sized resource and re-running the PSO precache.
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                m_pRenderer->RefreshShadowDependentResources();
            }
            ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowParameter.shadowDepthBias, 0, 1);
            ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowParameter.shadowSlopeDepthBias, 0, 10);
            ImGui::SliderFloat("Shadow Max Slope Depth Bias",
                               &pUserData.shadowParameter.shadowMaxSlopeDepthBias,
                               0,
                               10);
            // Shadow filter radius in shadow map texels (fixed-radius PCF, UE non-PCSS style).
            ImGui::SliderFloat("Shadow Radius",
                               &pUserData.shadowParameter.shadowRadius,
                               0.0f,
                               5.0f,
                               "%.2f");
            ImGui::SliderFloat("Shadow Distance",
                               &pUserData.shadowParameter.shadowDistance,
                               1.f,
                               200.f,
                               "%.1f");
            ImGui::Checkbox("Enable Shadow TAA",
                            &pUserData.shadowParameter.EnableTAA);
        }

        if (ImGui::CollapsingHeader("PBR Data"))
        {
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
                    const DisplayMode selected = m_displayModesAvailable[m_currentDisplayModeNamesIndex];
                    const bool windowedHdr =
                        CheckIfWindowModeHdrOn() &&
                        (selected == DISPLAYMODE_SDR ||
                         selected == DISPLAYMODE_HDR10_2084 ||
                         selected == DISPLAYMODE_HDR10_SCRGB);
                    if (m_fullscreenMode != PRESENTATIONMODE_WINDOWED || windowedHdr)
                    {
                        UpdateDisplay(selected, m_disableLocalDimming);
                        m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                        UserData::GetInstance().hdrParameter.displayMode = selected;
                    }
                    else
                    {
                        m_currentDisplayModeNamesIndex = m_previousDisplayModeNamesIndex;
                    }
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
        if (SelectionManager::GetInstance().GetSelected() == entity)
            flags |= ImGuiTreeNodeFlags_Selected;

        // 如果没有子节点，标记为叶子节点（不显示箭头）
        bool hasChildren = !entity->GetChildren().empty();
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        // 2. 渲染节点
        // Viewport picking: force-open the ancestor chain so the selected node
        // becomes visible and can be scrolled to below.
        if (SelectionManager::GetInstance().IsScrollTargetAncestor(entity))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        // 使用指针作为唯一 ID，节点显示名称
        bool opened = ImGui::TreeNodeEx((void*)entity, flags, entity->name.c_str());

        // 3. 处理点击交互
        if (ImGui::IsItemClicked())
        {
            SelectionManager::GetInstance().Select(entity);
        }

        // 3.5 When the node got selected by a viewport click, scroll here
        //     (consumed once, only at the selected node)
        if (SelectionManager::GetInstance().GetSelected() == entity &&
            SelectionManager::GetInstance().ConsumeScrollToSelected())
        {
            ImGui::SetScrollHereY(0.5f);
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

        if (entity->pMeshRenderer == nullptr || entity->pMeshRenderer->m_pModel == nullptr)
            return;

        auto& model = *entity->pMeshRenderer->m_pModel;
        const UINT materialIndex = entity->pMeshRenderer->GetMesh().materialIndex;
        if (materialIndex >= model.materials.size())
            return;

        if (ImGui::CollapsingHeader("Material Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& material = model.materials[materialIndex];
            if (material.shadingModelID < 0)
                material.shadingModelID = static_cast<int>(ShadingModel::DefaultLit);

            ImGui::Text("Material: %s", material.name.empty() ? "Material" : material.name.c_str());
            bool edited = false;
            auto shadingModel = static_cast<ShadingModel>(material.shadingModelID);
            if (ElysiaRenderer::EnumCombo("Shading Model", &shadingModel))
            {
                material.shadingModelID = static_cast<int>(shadingModel);
                edited = true;
            }

            auto& userData = UserData::GetInstance();
            if (shadingModel == ShadingModel::Preintegrated_Skin)
            {
                if (ImGui::ColorEdit3("Subsurface Color",
                                      (float*)&material.subsurfaceColor,
                                      ImGuiColorEditFlags_HDR))
                    edited = true;
                ImGui::SliderFloat("Curve Scale", &userData.subsurfaceScatterParameter.CurveScale, 0.f, 2.f);
                ImGui::SliderFloat("Min Curve", &userData.subsurfaceScatterParameter.MinCurve, 0.f, 1.f);
                ImGui::SliderFloat("Scatter Radius", &userData.subsurfaceScatterParameter.ScatterRadius, 0.f, 2.f);
                ImGui::SliderFloat("Transmission Scale",
                                   &userData.subsurfaceScatterParameter.TransmissionScale,
                                   0.f,
                                   5.f);
                ImGui::SliderFloat("Transmission Range",
                                   &userData.subsurfaceScatterParameter.TransmissionRange,
                                   0.f,
                                   2.f);
                ImGui::SliderFloat("Transmission Edge Glow",
                                   &userData.subsurfaceScatterParameter.TransmissionEdgeGlow,
                                   0.f,
                                   1.f);
            }
            if (shadingModel == ShadingModel::Hair)
            {
                if (ImGui::Checkbox("Enable Multi Scatter", &userData.hairParameter.bEnableMultiScatter))
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                if (ImGui::Checkbox("Enable R", &userData.hairParameter.bEnableR))
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                if (ImGui::Checkbox("Enable TT", &userData.hairParameter.bEnableTT))
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                if (ImGui::Checkbox("Enable TRT", &userData.hairParameter.bEnableTRT))
                    m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                if (ImGui::SliderFloat("Back Lit", &material.backLit, 0.f, 1.f))
                    edited = true;
            }

            const ImGuiColorEditFlags baseColorFlags =
                ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB;
            if (ImGui::ColorEdit3("Base Color", (float*)&material.albedoFactor, baseColorFlags))
                edited = true;
            if (ImGui::SliderFloat("Opacity", &material.opacity, 0.f, 1.f))
                edited = true;
            if (ImGui::SliderFloat("Cutoff", &material.alphaCutoff, 0.f, 1.f))
                edited = true;
            if (ImGui::SliderFloat("Normal Intensity", &material.normalFactor, 0.f, 2.f))
                edited = true;
            if (ImGui::SliderFloat("Metallic", &material.metallicFactor, 0.f, 1.f))
                edited = true;
            if (ImGui::SliderFloat("Roughness", &material.roughnessFactor, 0.f, 1.f))
                edited = true;
            if (ImGui::SliderFloat("Specular", &material.specularFactor, 0.f, 1.f))
                edited = true;
            if (ImGui::ColorEdit3("Emission", (float*)&material.emissiveFactor))
                edited = true;

            if (edited)
                model.materialParametersDirty = true;
            if (model.materialParametersDirty && !ImGui::IsAnyItemActive())
                ElysiaModel::MaterialOverrides::SaveIfDirty(model);
        }
    }
    void ElysiaFrame::ApplyGizmoWorldMatrix(Entity* entity, const Matrix& worldMatrix)
    {
        // Transform::GetWorldMatrix() composes as local * parentWorld, so a
        // manipulated world matrix has to be converted back to local space before
        // it can be written into position / rotation / scale.
        Matrix localMatrix = worldMatrix;
        if (Entity* pParent = entity->GetParent())
        {
            localMatrix = worldMatrix * pParent->transform.GetWorldMatrix().Invert();
        }

        Vector3 scale, translation;
        Quaternion rotation;
        if (!localMatrix.Decompose(scale, rotation, translation))
            return;

        entity->transform.position = translation;
        entity->transform.rotation = rotation;
        entity->transform.scale = scale;

        // Same dirty path the Inspector uses, so the render list picks it up.
        entity->OnTransformChanged();
    }

    void ElysiaFrame::DrawGizmoToolbar(const ImVec2& imageOrigin)
    {
        // Overlay (absolute position) so the viewport image keeps its layout and
        // its render targets are not resized by adding a toolbar row.
        ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 10.0f));
        ImGui::BeginGroup();

        const auto operationButton = [this](const char* label, int operation)
        {
            const bool bActive = (m_gizmoOperation == operation);
            if (bActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
            }
            const bool bClicked = ImGui::Button(label, ImVec2(56.0f, 0.0f));
            if (bActive)
            {
                ImGui::PopStyleColor();
            }
            if (bClicked)
            {
                m_gizmoOperation = operation;
            }
        };

        operationButton("Move", ImGuizmo::TRANSLATE);
        ImGui::SameLine();
        operationButton("Rotate", ImGuizmo::ROTATE);
        ImGui::SameLine();
        operationButton("Scale", ImGuizmo::SCALE);

        ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 40.0f));
        const bool bLocal = (m_gizmoMode == ImGuizmo::LOCAL);
        if (bLocal)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.85f));
        }
        if (ImGui::Button(bLocal ? "Local" : "World", ImVec2(64.0f, 0.0f)))
        {
            m_gizmoMode = bLocal ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }
        if (bLocal)
        {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_gizmoUseSnap);

        ImGui::EndGroup();
    }

    void ElysiaFrame::DrawShadowFrustumOverlay(const ImVec2& imageOrigin, const ImVec2& imageSize)
    {
        auto* pCamera = CameraManager::GetInstance().GetMainCamera();
        DX12Shadow* pShadow = LightManager::GetInstance().GetMainShadow();
        if (pCamera == nullptr || pShadow == nullptr || imageSize.x <= 0.0f || imageSize.y <= 0.0f)
            return;

        // The shadow camera is a *different* camera, so its frustum is a real shape
        // in the scene. Unproject its 8 corners (D3D convention: z = 0 near, 1 far)
        // and project them with the viewport camera.
        const Matrix viewProj = pCamera->GetViewMat() * pCamera->GetProjMat();
        const Matrix shadowViewProj = pShadow->GetView() * pShadow->GetProj();
        const Matrix invShadowViewProj = shadowViewProj.Invert();

        // Corner index: bit0 = +x, bit1 = +y, bit2 = far plane.
        ImVec2 cornersScreen[8]{};
        bool bValid[8]{};
        for (int corner = 0; corner < 8; ++corner)
        {
            const float ndcX = ((corner & 1) != 0) ? 1.0f : -1.0f;
            const float ndcY = ((corner & 2) != 0) ? 1.0f : -1.0f;
            const float ndcZ = ((corner & 4) != 0) ? 1.0f : 0.0f;

            const Vector3 cornerWS = Vector3::Transform(Vector3(ndcX, ndcY, ndcZ), invShadowViewProj);
            const Vector4 cornerClip = Vector4::Transform(
                Vector4(cornerWS.x, cornerWS.y, cornerWS.z, 1.0f),
                viewProj);

            bValid[corner] = cornerClip.w > 0.0f;
            if (!bValid[corner])
                continue;

            const float screenX = cornerClip.x / cornerClip.w;
            const float screenY = cornerClip.y / cornerClip.w;
            cornersScreen[corner] = ImVec2(imageOrigin.x + (screenX * 0.5f + 0.5f) * imageSize.x,
                                           imageOrigin.y + (0.5f - screenY * 0.5f) * imageSize.y);
        }

        // 12 edges of the frustum
        static constexpr int edges[12][2] =
        {
            {0, 1}, {1, 3}, {3, 2}, {2, 0}, // near plane
            {4, 5}, {5, 7}, {7, 6}, {6, 4}, // far plane
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // side edges
        };

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
        const ImU32 nearColor = IM_COL32(255, 205, 60, 255);
        const ImU32 farColor = IM_COL32(70, 170, 255, 190);

        for (const auto& edge : edges)
        {
            const int a = edge[0];
            const int b = edge[1];
            if (!bValid[a] || !bValid[b])
                continue;

            const bool bNearPlaneEdge = (a < 4) && (b < 4);
            pDrawList->AddLine(cornersScreen[a],
                               cornersScreen[b],
                               bNearPlaneEdge ? nearColor : farColor,
                               bNearPlaneEdge ? 2.0f : 1.5f);
        }

        // Mark the near-plane centre as the "eye" side of the frustum.
        if (bValid[0] && bValid[1] && bValid[2] && bValid[3])
        {
            const ImVec2 nearCenter((cornersScreen[0].x + cornersScreen[1].x +
                                     cornersScreen[2].x + cornersScreen[3].x) * 0.25f,
                                    (cornersScreen[0].y + cornersScreen[1].y +
                                     cornersScreen[2].y + cornersScreen[3].y) * 0.25f);
            pDrawList->AddCircleFilled(nearCenter, 3.0f, nearColor);
        }
    }

    void ElysiaFrame::DrawViewportGizmo(const ImVec2& imageOrigin, const ImVec2& imageSize)
    {
        DrawGizmoToolbar(imageOrigin);

        Entity* pSelected = SelectionManager::GetInstance().GetSelected();
        auto* pCamera = CameraManager::GetInstance().GetMainCamera();
        if (pSelected == nullptr || pCamera == nullptr)
        {
            m_bGizmoWasUsing = false;
            return;
        }

        // Draw the gizmo into the viewport window, over the scene image.
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(imageOrigin.x, imageOrigin.y, imageSize.x, imageSize.y);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetGizmoSizeClipSpace(0.12f);
        ImGuizmo::Enable(true);

        // Matrices are handed to ImGuizmo as-is (same convention as SimpleMath).
        float view[16]{};
        float proj[16]{};
        float object[16]{};
        ToGizmoMatrix(pCamera->GetViewMat(), view);
        ToGizmoMatrix(pCamera->GetProjMat(), proj);

        Matrix worldMatrix = pSelected->transform.GetWorldMatrix();

        Vector3 boundsMin{};
        Vector3 boundsMax{};
        bool bHasBounds = false;
        if (pSelected->pMeshRenderer != nullptr)
        {
            const BoundingBox localBounds = pSelected->pMeshRenderer->GetBoundingBox();
            const Vector3 centerOS = localBounds.Center;
            const Vector3 extentsOS = localBounds.Extents;
            if (std::isfinite(centerOS.x) && std::isfinite(centerOS.y) &&
                std::isfinite(centerOS.z) && std::isfinite(extentsOS.x) &&
                std::isfinite(extentsOS.y) && std::isfinite(extentsOS.z))
            {
                boundsMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
                boundsMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                for (int corner = 0; corner < 8; ++corner)
                {
                    const Vector3 cornerOS(
                        centerOS.x + ((corner & 1) != 0 ? extentsOS.x : -extentsOS.x),
                        centerOS.y + ((corner & 2) != 0 ? extentsOS.y : -extentsOS.y),
                        centerOS.z + ((corner & 4) != 0 ? extentsOS.z : -extentsOS.z));
                    const Vector3 cornerWS = Vector3::Transform(cornerOS, worldMatrix);
                    boundsMin = Vector3::Min(boundsMin, cornerWS);
                    boundsMax = Vector3::Max(boundsMax, cornerWS);
                }
                bHasBounds = true;
            }
        }

        const Matrix viewProj = pCamera->GetViewMat() * pCamera->GetProjMat();

        // Preferred pivot: the bounds centre, i.e. the object's centre as a user
        // expects. Only when that centre is outside the view (huge meshes that wrap
        // around the camera or extend far off screen) fall back to a point that is
        // guaranteed to be visible.
        Vector3 pivot = worldMatrix.Translation();
        if (bHasBounds)
        {
            const Vector3 boundsCenter = (boundsMin + boundsMax) * 0.5f;

            bool bCenterOnScreen = false;
            {
                const Vector4 centerClip = Vector4::Transform(
                    Vector4(boundsCenter.x, boundsCenter.y, boundsCenter.z, 1.0f),
                    viewProj);
                if (centerClip.w > 0.0f)
                {
                    const float ndcX = centerClip.x / centerClip.w;
                    const float ndcY = centerClip.y / centerClip.w;
                    bCenterOnScreen = ndcX >= -1.0f && ndcX <= 1.0f &&
                                      ndcY >= -1.0f && ndcY <= 1.0f;
                }
            }

            if (bCenterOnScreen)
            {
                pivot = boundsCenter;
            }
            else
            {
                const Vector3 cameraPos = pCamera->GetPosition();
                const Vector3 cameraDir = pCamera->GetForwardDir();
                const DirectX::BoundingBox worldBounds(boundsCenter,
                                                       (boundsMax - boundsMin) * 0.5f);

                if (worldBounds.Contains(cameraPos) == DirectX::CONTAINS)
                {
                    // Camera inside the object (interior walls): use the point where
                    // the view ray leaves the bounds - the surface being faced.
                    float exitDistance = FLT_MAX;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        const float direction = cameraDir[axis];
                        if (fabsf(direction) > 1.0e-4f)
                        {
                            const float bound = (direction > 0.0f)
                                                    ? boundsMax[axis]
                                                    : boundsMin[axis];
                            exitDistance = (std::min)(exitDistance,
                                                      (bound - cameraPos[axis]) / direction);
                        }
                    }
                    pivot = cameraPos + cameraDir * (std::max)(exitDistance, 0.0f);
                }
                else
                {
                    float hitDistance = 0.0f;
                    if (worldBounds.Intersects(cameraPos, cameraDir, hitDistance))
                    {
                        // Object under the view centre: keep the handle visible.
                        pivot = cameraPos + cameraDir * hitDistance;
                    }
                    else
                    {
                        // Otherwise the point of the bounds closest to the camera.
                        pivot = Vector3::Min(Vector3::Max(cameraPos, boundsMin), boundsMax);
                    }
                }
            }
        }

        Vector3 worldScale{}, worldTranslation{};
        Quaternion worldRotation{};
        if (!worldMatrix.Decompose(worldScale, worldRotation, worldTranslation))
        {
            worldScale = Vector3::One;
            worldRotation = Quaternion::Identity;
        }

        // Final safety net: if the chosen pivot ended up behind the camera, use the
        // transform origin so the handle stays reachable.
        Vector4 pivotClip = Vector4::Transform(Vector4(pivot.x, pivot.y, pivot.z, 1.0f), viewProj);
        if (pivotClip.w <= 0.0f)
        {
            pivot = worldMatrix.Translation();
            pivotClip = Vector4::Transform(Vector4(pivot.x, pivot.y, pivot.z, 1.0f), viewProj);
        }

        // Gizmo frame: same orientation/scale as the object, positioned at the pivot.
        const Matrix gizmoFrame = Matrix::CreateScale(worldScale) *
                                  Matrix::CreateFromQuaternion(worldRotation) *
                                  Matrix::CreateTranslation(pivot);
        ToGizmoMatrix(gizmoFrame, object);

        const float snap[3] = {m_gizmoSnapTranslate, m_gizmoSnapRotateDegrees, m_gizmoSnapScale};
        ImGuizmo::Manipulate(view,
                             proj,
                             static_cast<ImGuizmo::OPERATION>(m_gizmoOperation),
                             static_cast<ImGuizmo::MODE>(m_gizmoMode),
                             object,
                             nullptr,
                             m_gizmoUseSnap ? snap : nullptr);

        const bool bUsing = ImGuizmo::IsUsing();
        if (bUsing)
        {
            if (!m_bGizmoWasUsing && m_pGizmoIdleEntity == pSelected)
            {
                // Drag start with a valid pre-drag snapshot: remember it for undo.
                m_gizmoUndoStack.push_back({pSelected, m_gizmoIdleTransform});
                if (m_gizmoUndoStack.size() > GizmoUndoStackLimit)
                {
                    m_gizmoUndoStack.erase(m_gizmoUndoStack.begin());
                }
            }

            // The gizmo may sit on an off-centre pivot, so apply its delta
            // (frame^-1 * frame') to the object instead of overwriting the matrix.
            const Matrix manipulatedFrame = FromGizmoMatrix(object);
            const Matrix newWorldMatrix = worldMatrix * gizmoFrame.Invert() * manipulatedFrame;
            ApplyGizmoWorldMatrix(pSelected, newWorldMatrix);
        }
        else
        {
            // Keep the pre-drag state current so the next drag can snapshot it.
            m_gizmoIdleTransform = pSelected->transform;
            m_pGizmoIdleEntity = pSelected;
        }
        m_bGizmoWasUsing = bUsing;

        // Shortcuts, only while the viewport is focused and the camera is not
        // being flown (RMB) or text is being typed.
        const bool bViewportActive = ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput &&
                                     !ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!bViewportActive)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_W))
            m_gizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            m_gizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R))
            m_gizmoOperation = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_X))
        {
            m_gizmoMode = (m_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !m_gizmoUndoStack.empty())
        {
            GizmoUndoEntry& entry = m_gizmoUndoStack.back();
            if (entry.pEntity != nullptr)
            {
                entry.pEntity->transform = entry.transform;
                entry.pEntity->OnTransformChanged();
            }
            m_gizmoUndoStack.pop_back();
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