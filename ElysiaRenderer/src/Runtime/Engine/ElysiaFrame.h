#pragma once
#include "FrameworkWindows.h"
#include "Editor/UI.h"
#include "Runtime/RenderCore/Renderer.h"

struct ImGuiIO;

namespace ElysiaEngine
{
    class ElysiaFrame : public FrameworkWindows
    {
    public:
        ElysiaFrame(eastl::wstring name);
        void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight) override;
        void OnCreate() override;
        void OnDestroy() override;
        void OnRender() override;
        bool OnEvent(MSG msg) override;
        void OnResize() override;
        void OnUpdateDisplay() override;

        void OnUpdate();

        void HandleInput(const ImGuiIO& io);

    private:
        bool                        m_bIsBenchmarking;
        bool                        m_loadingScene = false;
        ElysiaRenderer::Renderer*   m_pRenderer = NULL;
        float                       m_fontSize;

        float                       m_time; // Time accumulator in seconds, used for animation.
        std::vector<std::string>    m_sceneNames;
        bool                        m_bPlay;

        ElysiaEditor::UIState       m_UIState;

    };
}

