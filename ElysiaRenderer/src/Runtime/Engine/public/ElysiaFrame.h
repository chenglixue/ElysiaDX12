#pragma once
#include "FrameworkWindows.h"
#include "Editor/public/UI.h"
#include "Runtime/RenderCore/public/Renderer.h"

namespace ElysiaRenderer
{
    class FirstPersonCamera;
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

        void BuildUI();

    private:
        bool m_bIsBenchmarking;
        bool m_loadingScene = false;
        std::unique_ptr<ElysiaCore::DX12GraphicsContext> m_pGraphicsContext = nullptr;
        ElysiaRenderer::Renderer* m_pRenderer = NULL;
        float m_fontSize;

        float m_time; // Time accumulator in seconds, used for animation.
        std::vector<std::string> m_sceneNames;
        bool m_bPlay;

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

        struct GizmoUndoEntry
        {
            Entity* pEntity = nullptr;
            Transform transform{};
        };
        std::vector<GizmoUndoEntry> m_gizmoUndoStack;

        void SetupDockSpace();
        void BuildUISceneHierarchy();
        void BuildUIViewport();
        void BuildUIInspector();
        void BuildMainMenuBar();
        void BuildUIRenderSetting();

        void DrawEntityNode(Entity* entity);
        void DrawTransformComponent(Entity* entity);

        // Gizmo + its small overlay toolbar, drawn on top of the viewport image
        void DrawViewportGizmo(const ImVec2& imageOrigin, const ImVec2& imageSize);
        void DrawGizmoToolbar(const ImVec2& imageOrigin);
        // Writes a manipulated world matrix back into the entity's local transform
        void ApplyGizmoWorldMatrix(Entity* entity, const Matrix& worldMatrix);
    };
}