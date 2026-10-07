#pragma once
#include "ThirdParty/imgui/imgui.h"
#include "Runtime/RenderCore/public/TextureManager.h"

namespace ElysiaCore
{
    class DX12Device;
}

namespace ElysiaEditor
{
    // UE 5.7 Starship icons used by features we actually have.
    enum class EditorIcon : UINT
    {
        Move = 0,
        Rotate,
        Scale,
        World,
        Local,
        Snap,
        SnapTranslate,
        SnapRotate,
        SnapScale,
        CameraSpeed,
        Play,
        Stop,
        Outliner,
        Details,
        OutputLog,
        Lit,
        Unlit,
        BufferVisualization,
        LightingOnly,
        VirtualShadowMap,
        Lumen,
        Count
    };

    class EditorIcons
    {
    public:
        static EditorIcons& Get();

        static constexpr float DisplaySize = 16.0f;
        // UE dock tab: icon at text size, ~4px before the label.
        static constexpr float TabIconGap = 4.0f;

        void Init(ElysiaCore::DX12Device* pDevice);
        void Shutdown();

        bool IsValid(EditorIcon icon) const;
        ImTextureID GetTexID(EditorIcon icon) const;
        bool Button(EditorIcon icon, const char* id, const char* tooltip, bool bActive = false,
                    ImVec4 tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), float size = DisplaySize);
        bool IconLabelButton(EditorIcon icon, const char* label, const char* tooltip, bool bActive = false,
                             bool bCaret = false, const char* id = nullptr);
        void Image(EditorIcon icon);
        void DecorateWindowTab(EditorIcon icon);
        // "  Label###Label" with just enough leading spaces for the tab icon + gap.
        static const char* TabWindowName(const char* label);

    private:
        ElysiaCore::DX12Device* m_pDevice = nullptr;
        ElysiaRenderer::TextureManager::Handle m_handles[static_cast<UINT>(EditorIcon::Count)]{};
        ImTextureID m_texIds[static_cast<UINT>(EditorIcon::Count)]{};
        bool m_ready[static_cast<UINT>(EditorIcon::Count)]{};
    };
}
