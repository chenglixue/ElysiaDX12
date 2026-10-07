#pragma once
#include "FrameworkWindows.h"
#include "Editor/public/UI.h"
#include "Programs/public/DebugUtility.h"
#include "Runtime/RenderCore/public/Renderer.h"

namespace ElysiaRenderer
{
    class FirstPersonCamera;
    enum class BasicShapeType : uint8_t;
}

struct ImGuiIO;

namespace ElysiaEngine
{
    struct Entity;
    class ElysiaFrame : public FrameworkWindows
    {
    public:
        ElysiaFrame(std::wstring name);
        void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight) override;
        void OnCreate() override;
        void OnDestroy() override;
        void OnRender() override;
        bool OnEvent(MSG msg) override;
        void OnResize() override;
        void OnUpdateDisplay() override;
        void ReleaseResource() override;

        void OnUpdate();

        void HandleInput(const ImGuiIO& io);
        // UE FEditorViewportCommands::FocusViewportToSelection (F).
        void FocusViewportToSelection();
        // UE FLevelViewportCommands::ToggleImmersive (F11).
        void ToggleViewportImmersive();

        void BuildUI();

    private:
        void RestoreSavedDisplayMode();
        bool m_bIsBenchmarking;
        bool m_loadingScene = false;
        std::unique_ptr<ElysiaCore::DX12GraphicsContext> m_pGraphicsContext = nullptr;
        ElysiaRenderer::Renderer* m_pRenderer = NULL;
        float m_fontSize;

        float m_time; // Time accumulator in seconds, used for animation.
        std::vector<std::string> m_sceneNames;
        bool m_bViewportHovered = false;
        // UE ShouldOrbitCamera + IsOrbitRotationMode: Alt+LMB drag around LookAt.
        bool m_bOrbiting = false;
        bool m_bOutputLogButtonHovered = false;
        bool m_bOutputLogWasOpen = false;
        // Last Viewport widget client size (FSceneViewport DrawSize). Scene RTs
        // and camera aspect follow this, not the OS client rectangle.
        uint32_t m_viewportClientWidth = 0;
        uint32_t m_viewportClientHeight = 0;
        bool m_bSceneViewportResourcesDirty = false;
        Vector3 m_playCameraPosition = Vector3::Zero;
        float m_playCameraPitch = 0.f;
        float m_playCameraYaw = 0.f;

        ElysiaEditor::UIState m_UIState;
        std::unique_ptr<ElysiaEditor::IMGUIDrawer> m_pImGui = nullptr;

        // ---- Viewport transform gizmo (ImGuizmo) ----
        // Operation / mode are kept as plain ints (ImGuizmo::TRANSLATE/ROTATE/SCALE,
        // WORLD/LOCAL) so this header does not need to include ImGuizmo.
        int m_gizmoOperation = 1;
        int m_gizmoMode = 0;
        bool m_gizmoUseSnap = false;
        float m_gizmoSnapTranslate = 0.5f;
        float m_gizmoSnapRotateDegrees = 15.0f;
        float m_gizmoSnapScale = 0.1f;
        bool m_bGizmoWasUsing = false;

        // Transform as it was before the current (or last) drag, so a drag can
        // push an undo entry on its first frame. The entity owning that snapshot
        // is tracked too, so a selection change cannot feed the wrong transform
        // into the undo stack.
        Transform m_gizmoIdleTransform{};
        Entity* m_pGizmoIdleEntity = nullptr;

        // Editor overlay: draw the shadow (light) camera's frustum inside the
        // viewport. Drawing the *main* camera's own frustum from its own viewpoint
        // is degenerate (the corners project onto the viewport border), so the
        // overlay shows the light camera instead - which is the useful one for
        // judging shadow coverage.
        bool m_bShowShadowFrustum = false;
        // Window -> Reset Layout: rebuild the UE default dock on the next SetupDockSpace.
        bool m_bResetEditorLayout = false;
        // UE SLevelViewport immersive: Viewport overlays the whole editor window.
        // Not OS exclusive/borderless fullscreen (that is Alt+Enter).
        bool m_bViewportImmersive = false;
        // Toolbar View Mode while a Render Settings overlay (GIProbe, Bloom, ...) is active.
        ElysiaRenderer::DebugMode m_lastViewportViewMode = ElysiaRenderer::DebugMode::None;

        struct GizmoUndoEntry
        {
            Entity* pEntity = nullptr;
            Transform transform{};
        };
        std::vector<GizmoUndoEntry> m_gizmoUndoStack;

        void SetupDockSpace();
        // UE FSceneViewport::OnDrawViewport: recreate scene RTs + camera aspect
        // to match the Viewport widget. Must run before 3D, never inside UIPass.
        void SyncSceneViewportSize();
        void BuildUISceneHierarchy();
        void BuildUIViewport();
        // Scene image + gizmo inside the current Viewport window.
        void DrawViewportImage();
        // UE SWindow::SetFullWindowOverlayContent: cover the editor, keep the dock tree.
        void BuildImmersiveViewportOverlay();
        void BuildUIInspector();
        void BuildMainMenuBar();
        void BuildUIRenderSetting();
        // UE row 2: Level Editor toolbar (Play / Stop). Full-width under the menu, never over the image.
        void DrawEditorToolbar();
        // UE SStatusBar: full-width row under the dock. Output Log is a drawer, not a docked tab.
        void DrawStatusBar();

        void DrawEntityNode(Entity* entity);
        void DrawTransformComponent(Entity* entity);
        void DrawLightComponent(Entity* entity);
        void PlaceDirectionalLight();
        void PlaceBasicShape(ElysiaRenderer::BasicShapeType type);
        void StartPlay();
        void StopPlay();

        // Gizmo drawn on the viewport image. Transform tools sit in the viewport toolbar row.
        void DrawViewportGizmo(const ImVec2& imageOrigin, const ImVec2& imageSize);
        // Editor overlay: unprojects the shadow camera's 8 frustum corners and
        // draws the 12 edges through the main camera (no GPU work).
        void DrawShadowFrustumOverlay(const ImVec2& imageOrigin, const ImVec2& imageSize);
        // UE row 3: Viewport toolbar (Move / Rotate / Scale / snap). Viewport menu bar, not over the image.
        void DrawGizmoToolbar();
        // UE viewport toolbar right: Camera Speed number + dropdown slider.
        void DrawCameraSpeedControl();
        // UE CreateViewModesSubmenu: Lit / Unlit / Buffer Visualization, right of Camera Speed.
        void DrawViewModeControl();
        // Writes a manipulated world matrix back into the entity's local transform
        void ApplyGizmoWorldMatrix(Entity* entity, const Matrix& worldMatrix);
    };
}