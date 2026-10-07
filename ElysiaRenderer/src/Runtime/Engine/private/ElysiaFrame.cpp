#include "stdafx.h"
#include "../public/ElysiaFrame.h"


#include "../public/ImGuiUtility.h"
#include "Editor/public/EditorIcons.h"
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;

    static void ToggleBool(bool& b)
    {
        b = !b;
    }

    namespace
    {
        ImGuiWindowClass MakeViewportWindowClass()
        {
            // UE SLevelEditor viewport stack: HideTabWell, no split over the
            // document, no independent Y splitter (only left/right neighbors).
            ImGuiWindowClass windowClass;
            windowClass.DockNodeFlagsOverrideSet =
                ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingSplit |
                ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoResizeY;
            return windowClass;
        }

        // UE LevelEditor_Layout_v1.8: bump when the default dock tree is incompatible.
        constexpr const char* kEditorDockSpace = "ElysiaLevelEditor_Layout_v1";

        void ApplyDefaultEditorDockLayout(ImGuiID dockspace_id, const ImVec2& dockSize)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, dockSize);

            ImGuiID dock_id_left = 0;
            ImGuiID dock_id_center = 0;
            ImGuiID dock_id_right = 0;
            ImGui::DockBuilderSplitNode(
                dockspace_id, ImGuiDir_Left, 0.16f, &dock_id_left, &dock_id_center);
            ImGui::DockBuilderSplitNode(
                dock_id_center, ImGuiDir_Right, 0.22f, &dock_id_right, &dock_id_center);

            ImGuiID dock_id_details = 0;
            ImGuiID dock_id_render_settings = 0;
            ImGui::DockBuilderSplitNode(
                dock_id_right, ImGuiDir_Up, 0.50f, &dock_id_details, &dock_id_render_settings);

            char outlinerName[96];
            char detailsName[96];
            std::snprintf(
                outlinerName,
                sizeof(outlinerName),
                "%s",
                ElysiaEditor::EditorIcons::TabWindowName("Outliner"));
            std::snprintf(
                detailsName,
                sizeof(detailsName),
                "%s",
                ElysiaEditor::EditorIcons::TabWindowName("Details"));

            ImGui::DockBuilderDockWindow(outlinerName, dock_id_left);
            ImGui::DockBuilderDockWindow("Render Settings", dock_id_render_settings);
            ImGui::DockBuilderDockWindow("Viewport", dock_id_center);
            ImGui::DockBuilderDockWindow(detailsName, dock_id_details);

            if (ImGuiDockNode* center = ImGui::DockBuilderGetNode(dock_id_center))
            {
                center->SetLocalFlags(
                    center->LocalFlags | ImGuiDockNodeFlags_CentralNode |
                    ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingSplit |
                    ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoResizeY);
            }

            ImGui::DockBuilderFinish(dockspace_id);
        }

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
        ElysiaEditor::EditorIcons::Get().Init(m_pDevice);

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
        StopPlay();
        ModelManager::GetInstance().FlushMaterialEdits();
        m_pDevice->WaitForIdle();
        ElysiaEditor::EditorIcons::Get().Shutdown();
        ElysiaEditor::ImGUI_Shutdown();
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
            // Scene color/depth follow the Viewport widget (FSceneViewport), not
            // the OS client size. Recreating them here would squash the image and
            // reset camera aspect to the window instead of the widget.
            if (CameraManager::GetInstance().GetMainCamera() == nullptr)
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
            }

            // Bootstrap RTs so 3D can run before the first Viewport layout.
            // Subsequent sizes come from SyncSceneViewportSize().
            if (m_pRenderer->GetDisplayRT() == nullptr)
            {
                m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
            }
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

        // Apply last frame's Viewport widget size before 3D, matching
        // FSceneViewport::OnDrawViewport (resize then Draw).
        if (!m_loadingScene)
            SyncSceneViewportSize();

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
                SyncSceneViewportSize();
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

    void ElysiaFrame::FocusViewportToSelection()
    {
        Entity* pSelected = SelectionManager::GetInstance().GetSelected();
        auto* pCamera = dynamic_cast<FirstPersonCamera*>(
            CameraManager::GetInstance().GetMainCamera());
        if (pSelected == nullptr || pCamera == nullptr)
            return;

        BoundingBox box = pSelected->GetWorldAABB();
        const Vector3 center = box.Center;
        const Vector3 extents = box.Extents;
        if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z) ||
            !std::isfinite(extents.x) || !std::isfinite(extents.y) || !std::isfinite(extents.z))
        {
            box.Center = pSelected->transform.GetWorldMatrix().Translation();
            box.Extents = Vector3(0.1f, 0.1f, 0.1f);
        }

        pCamera->FocusViewportOnBox(box);
    }
    void ElysiaFrame::ToggleViewportImmersive()
    {
        m_bViewportImmersive = !m_bViewportImmersive;
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
        const bool bPlaying = SceneManager::GetInstance().IsPlaying();
        const bool bPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

        // UE FEditorViewportCommands::FocusViewportToSelection: F, no Ctrl/Shift.
        // Alt+F is allowed so "hold Alt, F, keep Alt, drag" works.
        if (!bPlaying && !io.WantTextInput && !bPopupOpen &&
            !io.KeyCtrl && !io.KeyShift &&
            ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            FocusViewportToSelection();
        }

        // UE FLevelViewportCommands::ToggleImmersive: F11, no modifiers (Mac is Ctrl+F11).
        // Not Alt+Enter OS fullscreen. Allowed during Play, same as the viewport command.
        if (!io.WantTextInput && !bPopupOpen &&
            !io.KeyCtrl && !io.KeyShift && !io.KeyAlt &&
            ImGui::IsKeyPressed(ImGuiKey_F11, false))
        {
            ToggleViewportImmersive();
        }

        // UE ShouldOrbitCamera: Alt && !Ctrl && !Shift && !flight look && !ortho.
        // UE IsOrbitRotationMode: LMB && !MMB && !RMB.
        const bool bAltOrbitChord =
            !bPlaying && !io.WantTextInput &&
            io.KeyAlt && !io.KeyCtrl && !io.KeyShift &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Middle);

        if (m_bOrbiting)
        {
            if (!bAltOrbitChord)
                m_bOrbiting = false;
        }
        else if (bAltOrbitChord && m_bViewportHovered && !m_bGizmoWasUsing)
        {
            m_bOrbiting = true;
            pFirstPersonCam->BeginOrbitCamera();
        }

        if (m_bOrbiting && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
        {
            // UE ConvertMovementToOrbitDragRot: Yaw = +mouseX, then
            // SetViewRotation(pitch, -yaw). Same pixel scale as RMB look.
            pFirstPersonCam->OrbitCamera(-io.MouseDelta.x * sensitivity,
                                         io.MouseDelta.y * sensitivity);
        }

        const bool bLook = !m_bOrbiting && !io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const bool bMove = !m_bOrbiting && !io.KeyAlt &&
                           (bLook || (bPlaying && m_bViewportHovered && !io.WantTextInput));
        const bool bMouseHeld =
            ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Middle);

        const bool bWasdHeld =
            ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_S) ||
            ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_D) ||
            ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Q);

        // UE ViewportCameraSpeedMouseWheelInteraction: mouse button down → ±10%.
        // Also while actually flying (WASD) so Play without RMB still works.
        if (!io.WantTextInput && io.MouseWheel != 0.0f && m_bViewportHovered &&
            (bMouseHeld || bWasdHeld))
        {
            pFirstPersonCam->AdjustCameraSpeed(io.MouseWheel > 0.0f ? 0.1f : -0.1f);
        }

        if (bMove)
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
        }

        if (bLook)
        {
            float x = pFirstPersonCam->GetXRotation();
            float y = pFirstPersonCam->GetYRotation();
            x += io.MouseDelta.y * sensitivity;
            y += io.MouseDelta.x * sensitivity;
            pFirstPersonCam->SetXRotation(x);
            pFirstPersonCam->SetYRotation(y);
        }

        if ((bMove || bLook || m_bOrbiting) && !bPlaying)
        {
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
        // UE chrome order: menu, then Level Editor toolbar, then dock (viewport toolbar lives in Viewport).
        BuildMainMenuBar();
        SetupDockSpace();
        BuildUISceneHierarchy();
        BuildUIViewport();
        BuildUIInspector();
        BuildUIRenderSetting();

        const bool bPlaying = SceneManager::GetInstance().IsPlaying();
        if (!ImGui::GetIO().WantTextInput)
        {
            // UE OpenOutputLogDrawer (Alt+~). Toggle before drawing so it opens this frame.
            if (ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
                m_UIState.bShowOutputLog = !m_UIState.bShowOutputLog;
        }

        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float barH = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
            if (m_UIState.outputLogDrawerHeight <= 0.0f)
                m_UIState.outputLogDrawerHeight = viewport->WorkSize.y * 0.33f;
            // UE SDrawerOverlay: about a third of the window, capped at 90%.
            const float maxH = (std::max)(140.0f, viewport->WorkSize.y * 0.90f - barH);
            m_UIState.outputLogDrawerHeight = std::clamp(m_UIState.outputLogDrawerHeight, 140.0f, maxH);

            const ImVec2 drawerPos(
                viewport->WorkPos.x,
                viewport->WorkPos.y + viewport->WorkSize.y - barH - m_UIState.outputLogDrawerHeight);
            const ImVec2 drawerSize(viewport->WorkSize.x, m_UIState.outputLogDrawerHeight);
            bool drawerHovered = false;
            float heightDelta = 0.0f;
            ElysiaEditor::DrawOutputLogDrawer(
                m_UIState.bShowOutputLog, drawerPos, drawerSize, drawerHovered, &heightDelta);
            if (heightDelta != 0.0f)
            {
                m_UIState.outputLogDrawerHeight = std::clamp(
                    m_UIState.outputLogDrawerHeight + heightDelta, 140.0f, maxH);
            }

            // UE SWidgetDrawer::OnGlobalFocusChanging: dismiss when clicking away,
            // but not on the same click that opened the drawer (Window menu / Alt+` / button).
            const bool openedThisFrame = m_UIState.bShowOutputLog && !m_bOutputLogWasOpen;
            const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
            if (m_UIState.bShowOutputLog &&
                !openedThisFrame &&
                !popupOpen &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !drawerHovered &&
                !m_bOutputLogButtonHovered)
            {
                m_UIState.bShowOutputLog = false;
            }
            m_bOutputLogWasOpen = m_UIState.bShowOutputLog;
        }

        // After every other editor window so it covers menu, dock, and drawers
        // the way SWindow::SetFullWindowOverlayContent covers the owner window.
        BuildImmersiveViewportOverlay();

        if (!ImGui::GetIO().WantTextInput)
        {
            if (bPlaying && ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
                !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            {
                StopPlay();
            }
            else if (!bPlaying && ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_P, false))
            {
                StartPlay();
            }

            if (!bPlaying && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                Entity* pSelected = SelectionManager::GetInstance().GetSelected();
                Entity* pRoot = pSelected;
                while (pRoot && pRoot->GetParent())
                    pRoot = pRoot->GetParent();
                if (pRoot && pRoot->GetParent() == nullptr && pRoot->sourceModelIndex < 0 &&
                    (pRoot->pLight || pRoot->sourceShapeIndex >= 0))
                {
                    SceneManager::GetInstance().DestroyRootEntity(pRoot);
                }
            }
        }
    }
    void ElysiaFrame::SetupDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float toolbarH = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
        const float statusH = toolbarH;
        const ImVec2 dockPos(viewport->WorkPos.x, viewport->WorkPos.y + toolbarH);
        const ImVec2 dockSize(
            viewport->WorkSize.x,
            (std::max)(0.0f, viewport->WorkSize.y - toolbarH - statusH));

        const ImGuiWindowFlags chromeFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

        // UE Level Editor toolbar: a full-width row under the main menu.
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarH));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::Begin("##EditorToolbar", nullptr, chromeFlags);
        DrawEditorToolbar();
        ImGui::End();

        // UE SStatusBar: full-width row at the bottom of the editor.
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusH));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusH));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::Begin("##EditorStatusBar", nullptr, chromeFlags);
        DrawStatusBar();
        ImGui::End();
        ImGui::PopStyleColor();

        ImGui::SetNextWindowPos(dockPos);
        ImGui::SetNextWindowSize(dockSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("MainDockHost", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        // ID is owned by the host window so it is stable across sessions, matching
        // UE FLayoutSaveRestore keyed by LevelEditor_Layout_v*.
        const ImGuiID dockspace_id = ImGui::GetID(kEditorDockSpace);
        if (m_bResetEditorLayout)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            m_bResetEditorLayout = false;
        }
        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        {
            ApplyDefaultEditorDockLayout(dockspace_id, dockSize);
            ElysiaEditor::ImGUI_SaveLayout();
        }

        ImGui::DockSpace(
            dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_NoDockingOverCentralNode);
        ImGui::End();
    }
    void ElysiaFrame::BuildUISceneHierarchy()
    {
        ImGui::Begin(ElysiaEditor::EditorIcons::TabWindowName("Outliner"));
        // Keep Begin/End while immersive so the dock node stays in imgui.ini.
        if (m_bViewportImmersive)
        {
            ImGui::End();
            return;
        }
        ElysiaEditor::EditorIcons::Get().DecorateWindowTab(ElysiaEditor::EditorIcon::Outliner);

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
    void ElysiaFrame::SyncSceneViewportSize()
    {
        if (!m_pRenderer || m_viewportClientWidth == 0 || m_viewportClientHeight == 0)
            return;

        auto* pDisplay = m_pRenderer->GetDisplayRT();
        const uint32_t rtW = pDisplay ? static_cast<uint32_t>(pDisplay->GetWidth()) : 0;
        const uint32_t rtH = pDisplay ? static_cast<uint32_t>(pDisplay->GetHeight()) : 0;
        const bool sizeChanged = (rtW != m_viewportClientWidth || rtH != m_viewportClientHeight);
        if (sizeChanged || m_bSceneViewportResourcesDirty)
        {
            if (m_pDevice)
                m_pDevice->WaitForIdle();
            m_pRenderer->OnCreateWindowSizeDependentResources(
                &m_swapChain, m_viewportClientWidth, m_viewportClientHeight);
            m_bSceneViewportResourcesDirty = false;
        }

        if (auto* pCam = dynamic_cast<PerspectiveCamera*>(
                CameraManager::GetInstance().GetMainCamera()))
        {
            const float aspect = static_cast<float>(m_viewportClientWidth) /
                                 static_cast<float>(m_viewportClientHeight);
            if (std::abs(pCam->GetAspect() - aspect) > 1.0e-4f)
                pCam->SetAspectRatio(aspect);
        }
    }

    void ElysiaFrame::DrawViewportImage()
    {
        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            m_viewportClientWidth = static_cast<uint32_t>(viewportSize.x + 0.5f);
            m_viewportClientHeight = static_cast<uint32_t>(viewportSize.y + 0.5f);
            if (m_viewportClientWidth == 0)
                m_viewportClientWidth = 1;
            if (m_viewportClientHeight == 0)
                m_viewportClientHeight = 1;
            // First frame runs BuildUI before 3D, so the RT must exist now.
            if (m_pRenderer && m_pRenderer->GetDisplayRT() == nullptr)
                SyncSceneViewportSize();
        }

        if (!m_pRenderer || !m_pDevice)
            return;

        auto cameraRT = m_pRenderer->GetDisplayRT();
        if (!cameraRT)
            return;

        auto srcCPUHandle = cameraRT->GetTexture()->GetSRVDescriptor().GetCPUHandle();
        auto dstDescriptor = m_pDevice->GetImguiDescriptor();
        m_pDevice->GetDevice()->CopyDescriptorsSimple(
            1,
            dstDescriptor.GetCPUHandle(),
            srcCPUHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );

        ImTextureID sceneTexID = (ImTextureID)dstDescriptor.GetGPUHandle().ptr;

        // 1:1 with the RT (FSceneViewport). If the widget moved this frame
        // before the RT caught up, fit uniformly instead of squash-stretch.
        ImVec2 imageSize = viewportSize;
        ImVec2 imageOffset(0.0f, 0.0f);
        const float rtW = static_cast<float>(cameraRT->GetWidth());
        const float rtH = static_cast<float>(cameraRT->GetHeight());
        if (rtW > 0.0f && rtH > 0.0f && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            const float rtAspect = rtW / rtH;
            const float widgetAspect = viewportSize.x / viewportSize.y;
            if (std::abs(rtAspect - widgetAspect) > 0.0005f)
            {
                if (widgetAspect > rtAspect)
                {
                    imageSize.x = viewportSize.y * rtAspect;
                    imageSize.y = viewportSize.y;
                }
                else
                {
                    imageSize.x = viewportSize.x;
                    imageSize.y = viewportSize.x / rtAspect;
                }
                imageOffset.x = (viewportSize.x - imageSize.x) * 0.5f;
                imageOffset.y = (viewportSize.y - imageSize.y) * 0.5f;
            }
        }

        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor.x + imageOffset.x, cursor.y + imageOffset.y));
        ImGui::Image(sceneTexID, imageSize, ImVec2(0, 0), ImVec2(1, 1));
        const ImVec2 imageOrigin = ImGui::GetItemRectMin();

        DrawViewportGizmo(imageOrigin, imageSize);

        if (m_bShowShadowFrustum && !SceneManager::GetInstance().IsPlaying())
        {
            DrawShadowFrustumOverlay(imageOrigin, imageSize);
        }
    }

    void ElysiaFrame::BuildUIViewport()
    {
        const ImGuiWindowClass viewportClass = MakeViewportWindowClass();
        ImGui::SetNextWindowClass(&viewportClass);
        ImGui::Begin("Viewport",
                     nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_MenuBar);
        // Keep the docked Viewport window so imgui.ini / the dock tree stay put.
        // Size and hover belong to the overlay while immersive.
        if (!m_bViewportImmersive)
        {
            m_bViewportHovered = ImGui::IsWindowHovered();

            // UE viewport toolbar: a real menu-bar row above the image, not a floating overlay.
            if (ImGui::BeginMenuBar())
            {
                DrawGizmoToolbar();
                ImGui::EndMenuBar();
            }

            DrawViewportImage();
        }
        ImGui::End();
    }

    void ElysiaFrame::BuildImmersiveViewportOverlay()
    {
        if (!m_bViewportImmersive)
            return;

        ImGuiViewport* mainVp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainVp->Pos);
        ImGui::SetNextWindowSize(mainVp->Size);
        ImGui::SetNextWindowViewport(mainVp->ID);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##ImmersiveViewport", nullptr, flags);
        ImGui::PopStyleVar(3);
        // Cover docked chrome, but never climb above View Mode / Camera Speed
        // popups: those are layer-0 windows, and a per-frame display-front
        // would hide them behind this opaque overlay.
        if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        m_bViewportHovered = ImGui::IsWindowHovered();

        if (ImGui::BeginMenuBar())
        {
            DrawGizmoToolbar();
            ImGui::EndMenuBar();
        }

        DrawViewportImage();
        ImGui::End();
    }
    void ElysiaFrame::BuildUIInspector()
    {
        ImGui::Begin(ElysiaEditor::EditorIcons::TabWindowName("Details"));
        if (m_bViewportImmersive)
        {
            ImGui::End();
            return;
        }
        ElysiaEditor::EditorIcons::Get().DecorateWindowTab(ElysiaEditor::EditorIcon::Details);

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

        const bool bPlaying = SceneManager::GetInstance().IsPlaying();
        if (bPlaying)
            ImGui::TextDisabled("Playing — scene edits are discarded on Stop.");
        ImGui::BeginDisabled(bPlaying);
        DrawTransformComponent(pSelectedObject);
        DrawLightComponent(pSelectedObject);
        ImGui::EndDisabled();

        ImGui::End();
    }
    void ElysiaFrame::PlaceDirectionalLight()
    {
        if (SceneManager::GetInstance().IsPlaying())
            return;
        Entity* pLight = SceneManager::GetInstance().SpawnDirectionalLight();
        if (pLight)
            SelectionManager::GetInstance().Select(pLight);
    }
    void ElysiaFrame::PlaceBasicShape(BasicShapeType type)
    {
        if (SceneManager::GetInstance().IsPlaying())
            return;
        Entity* pShape = SceneManager::GetInstance().SpawnBasicShape(type);
        if (pShape)
            SelectionManager::GetInstance().Select(pShape);
    }
    void ElysiaFrame::StartPlay()
    {
        if (m_loadingScene || SceneManager::GetInstance().IsPlaying())
            return;

        if (auto* pCamera = CameraManager::GetInstance().GetMainCamera())
        {
            m_playCameraPosition = pCamera->GetPosition();
            if (auto* pFirstPerson = dynamic_cast<FirstPersonCamera*>(pCamera))
            {
                m_playCameraPitch = pFirstPerson->GetXRotation();
                m_playCameraYaw = pFirstPerson->GetYRotation();
            }
        }

        SceneManager::GetInstance().BeginPlay();
        SelectionManager::GetInstance().Clear();
        m_bGizmoWasUsing = false;
        m_pGizmoIdleEntity = nullptr;
    }
    void ElysiaFrame::StopPlay()
    {
        if (!SceneManager::GetInstance().IsPlaying())
            return;

        SceneManager::GetInstance().EndPlay();

        if (auto* pCamera = CameraManager::GetInstance().GetMainCamera())
        {
            pCamera->SetPosition(m_playCameraPosition);
            if (auto* pFirstPerson = dynamic_cast<FirstPersonCamera*>(pCamera))
            {
                pFirstPerson->SetYRotation(m_playCameraYaw);
                pFirstPerson->SetXRotation(m_playCameraPitch);
            }
        }
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

            const bool bPlaying = SceneManager::GetInstance().IsPlaying();
            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("Output Log", "Alt+`", &m_UIState.bShowOutputLog);
                if (ImGui::MenuItem("Reset Layout"))
                    m_bResetEditorLayout = true;
                if (ImGui::BeginMenu("Place Actors"))
                {
                    ImGui::BeginDisabled(bPlaying);
                    if (ImGui::MenuItem("Directional Light"))
                        PlaceDirectionalLight();
                    if (ImGui::BeginMenu("Shapes"))
                    {
                        if (ImGui::MenuItem("Cube"))
                            PlaceBasicShape(BasicShapeType::Cube);
                        if (ImGui::MenuItem("Sphere"))
                            PlaceBasicShape(BasicShapeType::Sphere);
                        if (ImGui::MenuItem("Plane"))
                            PlaceBasicShape(BasicShapeType::Plane);
                        ImGui::EndMenu();
                    }
                    ImGui::EndDisabled();
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();
    }
    void ElysiaFrame::BuildUIRenderSetting()
    {
        ImGui::Begin("Render Settings");
        if (m_bViewportImmersive)
        {
            ImGui::End();
            return;
        }
        auto& pUserData = UserData::GetInstance();

        ImGui::Checkbox("Enable HIZ", &pUserData.EnableHIZ);
        ImGui::Text("GBuffer Render Count: %u", GBufferPass::m_renderCount);
        if (ImGui::CollapsingHeader("Debug"))
        {
            // Viewport Lit / Unlit / Buffer Visualization live on the viewport
            // toolbar (UE CreateViewModesSubmenu). This combo is only the extra
            // overlays that have no View Mode equivalent.
            {
                const bool overlayOn = IsDebugOverlayMode(pUserData.debugMode);
                const char* overlayPreview = "Off";
                std::string overlayName;
                if (overlayOn)
                {
                    overlayName = std::string(magic_enum::enum_name(pUserData.debugMode));
                    overlayPreview = overlayName.c_str();
                }
                if (ImGui::BeginCombo("Debug Overlay", overlayPreview))
                {
                    if (ImGui::Selectable("Off", !overlayOn))
                        pUserData.debugMode = m_lastViewportViewMode;
                    for (DebugMode overlay : kDebugOverlayModes)
                    {
                        const std::string name(magic_enum::enum_name(overlay));
                        if (ImGui::Selectable(name.c_str(), pUserData.debugMode == overlay))
                            pUserData.debugMode = overlay;
                    }
                    ImGui::EndCombo();
                }
            }
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
                    m_bSceneViewportResourcesDirty = true;
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
        if (ImGui::IsItemClicked() && !SceneManager::GetInstance().IsPlaying())
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
    void ElysiaFrame::DrawLightComponent(Entity* entity)
    {
        if (!entity || !entity->pLight)
            return;

        LightComponent& light = *entity->pLight;
        auto& scene = SceneManager::GetInstance();

        auto writeThroughIfMain = [&]()
        {
            if (scene.FindMainDirectionalLight() != entity)
                return;
            auto& data = UserData::GetInstance();
            data.lightColor = light.color;
            data.lightDir = GetDirectionalLightDirection(entity->transform);
            data.lightIntensity = light.intensity;
            data.lightSourceAngleDegrees = light.sourceAngleDegrees;
            data.shadowParameter = light.shadow;
        };

        auto refreshShadowLayoutIfMain = [&](ShadowType oldType, ShadowQuality oldQuality)
        {
            writeThroughIfMain();
            if (scene.FindMainDirectionalLight() != entity)
                return;
            if (light.shadow.shadowType != oldType)
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
            if (light.shadow.shadowQuality != oldQuality)
            {
                m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                m_pRenderer->RefreshShadowDependentResources();
            }
        };

        // UE ULightComponentBase + UDirectionalLightComponent Category = Light.
        // FDirectionalLightComponentDetails overrides Intensity to 0-150 lux.
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Intensity", &light.intensity, 0.f, 150.f, "%.3f lux");
            ImGui::ColorEdit3("Light Color", (float*)&light.color);
            ImGui::Checkbox("Cast Shadows", &light.shadow.EnableShadow);
            ImGui::SliderFloat("Source Angle", &light.sourceAngleDegrees, 0.f, 5.f, "%.3f deg");
            const Vector3 dir = GetDirectionalLightDirection(entity->transform);
            ImGui::Text("Direction  %.3f  %.3f  %.3f", dir.x, dir.y, dir.z);
            ImGui::TextDisabled("Rays travel along local +X (UE GetDirection).");

            if (ImGui::TreeNode("Advanced"))
            {
                const ShadowType oldType = light.shadow.shadowType;
                const ShadowQuality oldQuality = light.shadow.shadowQuality;
                bool layoutChanged = false;
                if (ElysiaRenderer::EnumCombo("Shadow Type", &light.shadow.shadowType))
                    layoutChanged = true;
                if (ElysiaRenderer::EnumCombo("Shadow Quality", &light.shadow.shadowQuality))
                    layoutChanged = true;
                ImGui::SliderFloat("Shadow Bias", &light.shadow.shadowDepthBias, 0.f, 1.f);
                ImGui::SliderFloat("Shadow Slope Bias", &light.shadow.shadowSlopeDepthBias, 0.f, 10.f);
                ImGui::SliderFloat("Shadow Max Slope Bias", &light.shadow.shadowMaxSlopeDepthBias, 0.f, 10.f);
                ImGui::SliderFloat("Shadow Radius", &light.shadow.shadowRadius, 0.f, 5.f, "%.2f");
                ImGui::TextDisabled("PCF radius in shadow-map texels (this renderer has no PCSS).");
                ImGui::Checkbox("Shadow TAA", &light.shadow.EnableTAA);
                if (layoutChanged)
                    refreshShadowLayoutIfMain(oldType, oldQuality);
                ImGui::TreePop();
            }
        }

        // UDirectionalLightComponent Category = CascadedShadowMaps.
        if (ImGui::CollapsingHeader("Cascaded Shadow Maps"))
        {
            ImGui::SliderFloat("Dynamic Shadow Distance",
                               &light.shadow.shadowDistance,
                               1.f,
                               200.f,
                               "%.1f");
            ImGui::TextDisabled("How far cascaded shadows cover, measured from the camera.");
        }

        // UDirectionalLightComponent Category = AtmosphereAndCloud.
        if (ImGui::CollapsingHeader("Atmosphere and Cloud"))
        {
            bool atmosphereSun = light.bAtmosphereSun;
            if (ImGui::Checkbox("Atmosphere Sun Light", &atmosphereSun))
            {
                const ShadowType oldType = UserData::GetInstance().shadowParameter.shadowType;
                const ShadowQuality oldQuality = UserData::GetInstance().shadowParameter.shadowQuality;
                if (atmosphereSun)
                    scene.SetAtmosphereSun(entity);
                else
                    light.bAtmosphereSun = false;

                Entity* pMain = scene.FindMainDirectionalLight();
                if (pMain && pMain->pLight)
                {
                    auto& data = UserData::GetInstance();
                    data.lightColor = pMain->pLight->color;
                    data.lightDir = GetDirectionalLightDirection(pMain->transform);
                    data.lightIntensity = pMain->pLight->intensity;
                    data.lightSourceAngleDegrees = pMain->pLight->sourceAngleDegrees;
                    data.shadowParameter = pMain->pLight->shadow;
                    if (data.shadowParameter.shadowType != oldType)
                        m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                    if (data.shadowParameter.shadowQuality != oldQuality)
                    {
                        m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
                        m_pRenderer->RefreshShadowDependentResources();
                    }
                }
            }
        }

        writeThroughIfMain();
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

        // Same dirty path Details uses, so the render list picks it up.
        entity->OnTransformChanged();
    }

    void ElysiaFrame::DrawEditorToolbar()
    {
        using ElysiaEditor::EditorIcon;
        auto& icons = ElysiaEditor::EditorIcons::Get();
        const bool bPlaying = SceneManager::GetInstance().IsPlaying();

        // UE Toolbar.BackplateLeftPlay / BackplateCenterStop: white Starship
        // glyphs, green / red foreground, no text labels.
        constexpr float kPlayStopSize = 20.0f;
        const ImVec4 playTint(0.35f, 0.85f, 0.40f, 1.0f);
        const ImVec4 stopTint(0.90f, 0.32f, 0.28f, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

        ImGui::BeginDisabled(bPlaying);
        if (icons.Button(EditorIcon::Play, "##Play", "Play In Viewport (Alt+P)",
                         false, playTint, kPlayStopSize))
            StartPlay();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!bPlaying);
        if (icons.Button(EditorIcon::Stop, "##Stop", "Stop (Esc)",
                         false, stopTint, kPlayStopSize))
            StopPlay();
        ImGui::EndDisabled();

        ImGui::PopStyleVar(2);
    }

    void ElysiaFrame::DrawGizmoToolbar()
    {
        // Called from the Viewport window's menu bar. Items layout horizontally;
        // Separator() becomes a vertical tick, matching UE's viewport toolbar.
        using ElysiaEditor::EditorIcon;
        auto& icons = ElysiaEditor::EditorIcons::Get();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));

        const bool bPlaying = SceneManager::GetInstance().IsPlaying();
        ImGui::BeginDisabled(bPlaying);

        if (icons.Button(EditorIcon::Move, "##GizmoMove", "Move (W)",
                         m_gizmoOperation == ImGuizmo::TRANSLATE))
            m_gizmoOperation = ImGuizmo::TRANSLATE;
        if (icons.Button(EditorIcon::Rotate, "##GizmoRotate", "Rotate (E)",
                         m_gizmoOperation == ImGuizmo::ROTATE))
            m_gizmoOperation = ImGuizmo::ROTATE;
        if (icons.Button(EditorIcon::Scale, "##GizmoScale", "Scale (R)",
                         m_gizmoOperation == ImGuizmo::SCALE))
            m_gizmoOperation = ImGuizmo::SCALE;

        ImGui::Separator();

        const bool bLocal = (m_gizmoMode == ImGuizmo::LOCAL);
        if (icons.Button(bLocal ? EditorIcon::Local : EditorIcon::World,
                         "##GizmoCoord",
                         "Coordinate system (X)"))
            m_gizmoMode = bLocal ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

        ImGui::Separator();

        if (icons.Button(EditorIcon::Snap, "##GizmoSnap", "Enable snapping", m_gizmoUseSnap))
            m_gizmoUseSnap = !m_gizmoUseSnap;

        ImGui::BeginDisabled(!m_gizmoUseSnap);
        auto snapField = [&](EditorIcon icon, const char* id, float* value, float speed,
                             float minV, float maxV, const char* fmt, const char* tooltip)
        {
            icons.Image(icon);
            ImGui::SetNextItemWidth(56.0f);
            ImGui::DragFloat(id, value, speed, minV, maxV, fmt);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tooltip);
        };
        snapField(EditorIcon::SnapTranslate, "##SnapT", &m_gizmoSnapTranslate,
                  0.05f, 0.01f, 50.f, "%.2f", "Translation snap");
        snapField(EditorIcon::SnapRotate, "##SnapR", &m_gizmoSnapRotateDegrees,
                  1.f, 1.f, 90.f, "%.0f deg", "Rotation snap (degrees)");
        snapField(EditorIcon::SnapScale, "##SnapS", &m_gizmoSnapScale,
                  0.01f, 0.01f, 5.f, "%.2f", "Scale snap");
        ImGui::EndDisabled();

        ImGui::EndDisabled();

        auto* pCamera = dynamic_cast<FirstPersonCamera*>(
            CameraManager::GetInstance().GetMainCamera());
        const float padX = ImGui::GetStyle().FramePadding.x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        float camW = 0.0f;
        if (pCamera != nullptr)
        {
            const float speed = pCamera->GetCameraSpeed();
            char speedLabel[32]{};
            std::snprintf(speedLabel, sizeof(speedLabel), (speed > 1.0f) ? "%.1f" : "%.3f", speed);
            const float iconBtnW = ElysiaEditor::EditorIcons::DisplaySize + padX * 2.0f;
            const float numWidth = (std::max)(
                ImGui::CalcTextSize(speedLabel).x + padX * 2.0f + 8.0f, 48.0f);
            camW = iconBtnW + spacing + numWidth;
        }

        DebugMode displayMode = IsViewportViewMode(UserData::GetInstance().debugMode)
                                    ? UserData::GetInstance().debugMode
                                    : m_lastViewportViewMode;
        const char* viewLabel = GetViewportViewModeLabel(displayMode);
        const float viewIconW = ElysiaEditor::EditorIcons::DisplaySize;
        const float viewW = padX * 2.0f + viewIconW + 4.0f
            + ImGui::CalcTextSize(viewLabel).x + 4.0f + 8.0f;
        const float immersiveW = ImGui::CalcTextSize("Immersive").x + padX * 2.0f
            + ImGui::CalcTextSize("F11").x + 12.0f;
        const float totalW = viewW + spacing + immersiveW + (camW > 0.0f ? camW + spacing : 0.0f);
        const float rightX = ImGui::GetWindowContentRegionMax().x - totalW;
        if (rightX > ImGui::GetCursorPosX() + 8.0f)
            ImGui::SetCursorPosX(rightX);

        DrawCameraSpeedControl();
        DrawViewModeControl();
        if (ImGui::MenuItem("Immersive", "F11", m_bViewportImmersive))
            ToggleViewportImmersive();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Immersive View: fill the editor window. F11 toggles.");
        ImGui::PopStyleVar(2);
    }

    void ElysiaFrame::DrawStatusBar()
    {
        auto& icons = ElysiaEditor::EditorIcons::Get();
        if (icons.IconLabelButton(
                ElysiaEditor::EditorIcon::OutputLog,
                "Output Log",
                "Opens the output log drawer. (Alt+`) toggles.",
                m_UIState.bShowOutputLog))
        {
            m_UIState.bShowOutputLog = !m_UIState.bShowOutputLog;
        }
        m_bOutputLogButtonHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    }

    void ElysiaFrame::DrawCameraSpeedControl()
    {
        auto* pCamera = dynamic_cast<FirstPersonCamera*>(CameraManager::GetInstance().GetMainCamera());
        if (pCamera == nullptr)
            return;

        // UE CreateCameraSpeedMenu: raised toolbar entry on the right, label is the speed number.
        constexpr float kUiMin = 0.33f;
        constexpr float kUiMax = 32.0f;

        const float speed = pCamera->GetCameraSpeed();
        const char* valueFmt = (speed > 1.0f) ? "%.1f" : "%.3f";
        char label[32]{};
        std::snprintf(label, sizeof(label), valueFmt, speed);

        auto& icons = ElysiaEditor::EditorIcons::Get();
        const float padX = ImGui::GetStyle().FramePadding.x;
        const float iconBtnW = ElysiaEditor::EditorIcons::DisplaySize + padX * 2.0f;
        const float numWidth = (std::max)(
            ImGui::CalcTextSize(label).x + padX * 2.0f + 8.0f,
            48.0f);

        const char* speedTooltip = "Camera Speed (WASD). Hold a mouse button and scroll to adjust.";
        if (icons.Button(ElysiaEditor::EditorIcon::CameraSpeed, "##CameraSpeedIcon", speedTooltip))
            ImGui::OpenPopup("##CameraSpeedPopup");

        char buttonId[40]{};
        std::snprintf(buttonId, sizeof(buttonId), "%s###CameraSpeed", label);
        if (ImGui::Button(buttonId, ImVec2(numWidth, 0.0f)))
            ImGui::OpenPopup("##CameraSpeedPopup");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", speedTooltip);

        const ImVec2 btnMax = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2(btnMax.x, btnMax.y + 2.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f), ImVec2(320.0f, FLT_MAX));
        if (ImGui::BeginPopup("##CameraSpeedPopup"))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Speed");
            ImGui::SameLine();

            float uiSpeed = std::clamp(pCamera->GetCameraSpeed(), kUiMin, kUiMax);
            const float valueWidth = 64.0f;
            ImGui::SetNextItemWidth((std::max)(80.0f, ImGui::GetContentRegionAvail().x - valueWidth - ImGui::GetStyle().ItemSpacing.x));
            if (ImGui::SliderFloat("##CameraSpeedSlider",
                                   &uiSpeed,
                                   kUiMin,
                                   kUiMax,
                                   "",
                                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat))
            {
                pCamera->SetCameraSpeed(uiSpeed);
            }

            ImGui::SameLine();
            float typedSpeed = pCamera->GetCameraSpeed();
            const char* typedFmt = (typedSpeed > 1.0f) ? "%.1f" : "%.3f";
            ImGui::SetNextItemWidth(valueWidth);
            if (ImGui::InputFloat("##CameraSpeedInput", &typedSpeed, 0.0f, 0.0f, typedFmt))
                pCamera->SetCameraSpeed(typedSpeed);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Hold either mouse button and use the scroll wheel to adjust the speed on the fly.");
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
    }

    void ElysiaFrame::DrawViewModeControl()
    {
        using ElysiaEditor::EditorIcon;
        auto& icons = ElysiaEditor::EditorIcons::Get();
        auto& debugMode = UserData::GetInstance().debugMode;
        if (IsViewportViewMode(debugMode))
            m_lastViewportViewMode = debugMode;

        const DebugMode displayMode = IsViewportViewMode(debugMode)
                                          ? debugMode
                                          : m_lastViewportViewMode;
        const char* label = GetViewportViewModeLabel(displayMode);
        EditorIcon icon = EditorIcon::Lit;
        if (displayMode == DebugMode::Emission)
            icon = EditorIcon::Unlit;
        else if (displayMode == DebugMode::LightingOnly)
            icon = EditorIcon::LightingOnly;
        else if (IsBufferVisualizationMode(displayMode))
            icon = EditorIcon::BufferVisualization;
        else if (IsShadowVisualizationMode(displayMode))
            icon = EditorIcon::VirtualShadowMap;
        else if (IsGIVisualizationMode(displayMode))
            icon = EditorIcon::Lumen;

        const char* tooltip = "View mode settings for the current viewport.";
        if (icons.IconLabelButton(icon, label, tooltip, false, true, "ViewMode"))
            ImGui::OpenPopup("##ViewModePopup");

        const ImVec2 btnMin = ImGui::GetItemRectMin();
        const ImVec2 btnMax = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2(btnMin.x, btnMax.y + 2.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, 0.0f), ImVec2(360.0f, FLT_MAX));
        if (!ImGui::BeginPopup("##ViewModePopup"))
            return;

        auto applyMode = [&](DebugMode mode)
        {
            debugMode = mode;
            if (IsViewportViewMode(mode))
                m_lastViewportViewMode = mode;
            ImGui::CloseCurrentPopup();
        };

        auto menuRadio = [](bool on)
        {
            const float h = ImGui::GetFrameHeight();
            const float r = 4.0f;
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
            const ImVec2 c(p.x + r + 2.0f, p.y + h * 0.5f);
            dl->AddCircle(c, r, col, 16, 1.15f);
            if (on)
                dl->AddCircleFilled(c, r - 2.1f, col, 16);
            ImGui::Dummy(ImVec2(r * 2.0f + 8.0f, h));
        };

        auto modeRow = [&](EditorIcon rowIcon, const char* rowLabel, DebugMode mode)
        {
            ImGui::PushID(rowLabel);
            menuRadio(debugMode == mode);
            ImGui::SameLine(0.0f, 0.0f);
            if (icons.IsValid(rowIcon))
            {
                icons.Image(rowIcon);
                ImGui::SameLine(0.0f, 4.0f);
            }
            if (ImGui::Selectable(rowLabel, debugMode == mode))
                applyMode(mode);
            ImGui::PopID();
        };

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted("VIEW MODE");
        ImGui::PopStyleColor();

        modeRow(EditorIcon::Lit, "Lit", DebugMode::None);
        modeRow(EditorIcon::Unlit, "Unlit", DebugMode::Emission);
        modeRow(EditorIcon::LightingOnly, "Lighting Only", DebugMode::LightingOnly);

        auto visSubmenu = [&](const char* id, EditorIcon menuIcon, const char* title, bool parentOn,
                              const BufferVisualizationEntry* entries, size_t count)
        {
            ImGui::PushID(id);
            menuRadio(parentOn);
            ImGui::SameLine(0.0f, 0.0f);
            if (icons.IsValid(menuIcon))
            {
                icons.Image(menuIcon);
                ImGui::SameLine(0.0f, 4.0f);
            }
            if (ImGui::BeginMenu(title))
            {
                for (size_t i = 0; i < count; ++i)
                {
                    const BufferVisualizationEntry& entry = entries[i];
                    ImGui::PushID(static_cast<int>(entry.mode));
                    menuRadio(debugMode == entry.mode);
                    ImGui::SameLine(0.0f, 0.0f);
                    if (ImGui::Selectable(entry.label, debugMode == entry.mode))
                        applyMode(entry.mode);
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }
            ImGui::PopID();
        };

        visSubmenu("BufferVisualizationMenu",
                   EditorIcon::BufferVisualization,
                   "Buffer Visualization",
                   IsBufferVisualizationMode(debugMode),
                   kBufferVisualizationModes,
                   sizeof(kBufferVisualizationModes) / sizeof(kBufferVisualizationModes[0]));
        visSubmenu("ShadowVisualizationMenu",
                   EditorIcon::VirtualShadowMap,
                   "Shadow",
                   IsShadowVisualizationMode(debugMode),
                   kShadowVisualizationModes,
                   sizeof(kShadowVisualizationModes) / sizeof(kShadowVisualizationModes[0]));
        visSubmenu("GIVisualizationMenu",
                   EditorIcon::Lumen,
                   "GI",
                   IsGIVisualizationMode(debugMode),
                   kGIVisualizationModes,
                   sizeof(kGIVisualizationModes) / sizeof(kGIVisualizationModes[0]));

        ImGui::EndPopup();
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
        if (SceneManager::GetInstance().IsPlaying())
            return;

        // Viewport click picking: LMB on the image selects the entity, syncing
        // Outliner and Details. The transform gizmo owns the mouse
        // while hovered or dragged, so picking stands down.
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool bMouseOnImage =
            mousePos.x >= imageOrigin.x && mousePos.x < imageOrigin.x + imageSize.x &&
            mousePos.y >= imageOrigin.y && mousePos.y < imageOrigin.y + imageSize.y;
        if (imageSize.x > 0.0f && imageSize.y > 0.0f &&
            bMouseOnImage &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::GetIO().KeyAlt && !m_bOrbiting &&
            !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
        {
            const Vector2 viewportUV((mousePos.x - imageOrigin.x) / imageSize.x,
                                     (mousePos.y - imageOrigin.y) / imageSize.y);
            SelectionManager::GetInstance().RequestPick(viewportUV);
        }

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
        // UE CanUseDragTool: orbit (Alt) owns the mouse, widget stands down.
        ImGuizmo::Enable(!ImGui::GetIO().KeyAlt && !m_bOrbiting);

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