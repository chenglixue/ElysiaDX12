#include "stdafx.h"
#include "../public/EditorIcons.h"

#include "Programs/public/Helper.h"
#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"
#include "ThirdParty/imgui/imgui_internal.h"

namespace ElysiaEditor
{
    namespace
    {
        const wchar_t* IconFileName(EditorIcon icon)
        {
            switch (icon)
            {
            case EditorIcon::Move:          return L"Editor\\Icons\\Move.png";
            case EditorIcon::Rotate:        return L"Editor\\Icons\\Rotate.png";
            case EditorIcon::Scale:         return L"Editor\\Icons\\Scale.png";
            case EditorIcon::World:         return L"Editor\\Icons\\World.png";
            case EditorIcon::Local:         return L"Editor\\Icons\\Local.png";
            case EditorIcon::Snap:          return L"Editor\\Icons\\Snap.png";
            case EditorIcon::SnapTranslate: return L"Editor\\Icons\\SnapTranslate.png";
            case EditorIcon::SnapRotate:    return L"Editor\\Icons\\SnapRotate.png";
            case EditorIcon::SnapScale:     return L"Editor\\Icons\\SnapScale.png";
            case EditorIcon::CameraSpeed:   return L"Editor\\Icons\\CameraSpeed.png";
            case EditorIcon::Play:          return L"Editor\\Icons\\Play.png";
            case EditorIcon::Stop:          return L"Editor\\Icons\\Stop.png";
            case EditorIcon::Outliner:      return L"Editor\\Icons\\Outliner.png";
            case EditorIcon::Details:       return L"Editor\\Icons\\Details.png";
            case EditorIcon::OutputLog:           return L"Editor\\Icons\\OutputLog.png";
            case EditorIcon::Lit:                 return L"Editor\\Icons\\Lit.png";
            case EditorIcon::Unlit:               return L"Editor\\Icons\\Unlit.png";
            case EditorIcon::BufferVisualization: return L"Editor\\Icons\\BufferVisualization.png";
            case EditorIcon::LightingOnly:        return L"Editor\\Icons\\LightingOnly.png";
            case EditorIcon::VirtualShadowMap:    return L"Editor\\Icons\\VirtualShadowMap.png";
            case EditorIcon::Lumen:               return L"Editor\\Icons\\Lumen.png";
            default:                              return L"";
            }
        }
    }

    EditorIcons& EditorIcons::Get()
    {
        static EditorIcons instance;
        return instance;
    }

    void EditorIcons::Init(ElysiaCore::DX12Device* pDevice)
    {
        Shutdown();
        m_pDevice = pDevice;
        if (m_pDevice == nullptr)
            return;

        WCHAR assetsPath[512]{};
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

        for (UINT i = 0; i < static_cast<UINT>(EditorIcon::Count); ++i)
        {
            const EditorIcon icon = static_cast<EditorIcon>(i);
            const std::wstring path = std::wstring(assetsPath) + IconFileName(icon);
            auto handle = ElysiaRenderer::TextureManager::GetInstance().LoadDynamicTexture(path, false);
            if (!handle.IsValid())
                continue;

            ElysiaCore::DX12TextureResource* pTex =
                ElysiaRenderer::TextureManager::GetInstance().GetTexture(handle);
            if (pTex == nullptr)
                continue;

            const UINT imguiIndex = ElysiaHelper::IMGUI_EDITOR_ICON_DESCRIPTOR_BASE + i;
            auto dst = m_pDevice->GetImGUIRenderHeap().GetReservedDescriptor(imguiIndex);
            m_pDevice->GetDevice()->CopyDescriptorsSimple(
                1,
                dst.GetCPUHandle(),
                pTex->GetSRVDescriptor().GetCPUHandle(),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_handles[i] = handle;
            m_texIds[i] = (ImTextureID)dst.GetGPUHandle().ptr;
            m_ready[i] = true;
        }
    }

    void EditorIcons::Shutdown()
    {
        for (UINT i = 0; i < static_cast<UINT>(EditorIcon::Count); ++i)
        {
            m_handles[i] = {};
            m_texIds[i] = {};
            m_ready[i] = false;
        }
        m_pDevice = nullptr;
    }

    bool EditorIcons::IsValid(EditorIcon icon) const
    {
        const UINT i = static_cast<UINT>(icon);
        return i < static_cast<UINT>(EditorIcon::Count) && m_ready[i];
    }

    ImTextureID EditorIcons::GetTexID(EditorIcon icon) const
    {
        return IsValid(icon) ? m_texIds[static_cast<UINT>(icon)] : ImTextureID{};
    }

    bool EditorIcons::Button(EditorIcon icon, const char* id, const char* tooltip, bool bActive,
                             ImVec4 tint, float size)
    {
        if (size <= 0.0f)
            size = DisplaySize;

        if (bActive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        bool pressed = false;
        if (IsValid(icon))
        {
            pressed = ImGui::ImageButton(
                id,
                m_texIds[static_cast<UINT>(icon)],
                ImVec2(size, size),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
                tint);
        }
        else
        {
            pressed = ImGui::SmallButton(id);
        }

        ImGui::PopStyleColor();

        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
        return pressed;
    }

    void EditorIcons::Image(EditorIcon icon)
    {
        if (!IsValid(icon))
            return;
        ImGui::Image(m_texIds[static_cast<UINT>(icon)], ImVec2(DisplaySize, DisplaySize));
    }

    bool EditorIcons::IconLabelButton(EditorIcon icon, const char* label, const char* tooltip, bool bActive,
                                      bool bCaret, const char* id)
    {
        ImGui::PushID(id ? id : (label ? label : "IconLabel"));
        if (bActive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        const float iconSz = DisplaySize;
        const ImVec2 labelSize = ImGui::CalcTextSize(label ? label : "");
        const ImVec2 pad = ImGui::GetStyle().FramePadding;
        const float gap = 4.0f;
        const float caretW = bCaret ? 8.0f : 0.0f;
        const float width = pad.x * 2.0f
            + (IsValid(icon) ? iconSz + gap : 0.0f)
            + labelSize.x
            + (bCaret ? gap + caretW : 0.0f);
        const float height = ImGui::GetFrameHeight();

        const bool pressed = ImGui::Button("##IconLabel", ImVec2(width, height));
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float x = rmin.x + pad.x;
        const float midY = (rmin.y + rmax.y) * 0.5f;
        if (IsValid(icon))
        {
            const ImVec2 ip(x, midY - iconSz * 0.5f);
            dl->AddImage(m_texIds[static_cast<UINT>(icon)], ip, ImVec2(ip.x + iconSz, ip.y + iconSz));
            x += iconSz + gap;
        }
        dl->AddText(ImVec2(x, midY - labelSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label ? label : "");
        x += labelSize.x;
        if (bCaret)
        {
            x += gap;
            const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
            dl->AddTriangleFilled(
                ImVec2(x, midY - 2.5f),
                ImVec2(x + 7.0f, midY - 2.5f),
                ImVec2(x + 3.5f, midY + 2.5f),
                col);
        }

        ImGui::PopStyleColor();
        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tooltip);
        ImGui::PopID();
        return pressed;
    }

    const char* EditorIcons::TabWindowName(const char* label)
    {
        static char buf[96];
        if (label == nullptr)
            label = "";

        const float iconSz = ImGui::GetFontSize();
        const float need = iconSz + TabIconGap;
        float spaceW = ImGui::CalcTextSize(" ").x;
        if (spaceW < 1.0f)
            spaceW = 4.0f;
        int n = (int)((need + spaceW - 0.01f) / spaceW);
        if (n < 1)
            n = 1;
        if (n > 8)
            n = 8;

        char* p = buf;
        const char* end = buf + sizeof(buf) - 1;
        for (int i = 0; i < n && p < end; ++i)
            *p++ = ' ';
        for (const char* s = label; *s && p < end; ++s)
            *p++ = *s;
        if (p + 3 < end)
        {
            *p++ = '#';
            *p++ = '#';
            *p++ = '#';
        }
        for (const char* s = label; *s && p < end; ++s)
            *p++ = *s;
        *p = '\0';
        return buf;
    }

    void EditorIcons::DecorateWindowTab(EditorIcon icon)
    {
        if (!IsValid(icon))
            return;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr)
            return;

        const float iconSz = ImGui::GetFontSize();
        const char* name = window->Name ? window->Name : "";
        const char* label = name;
        while (*label == ' ')
            ++label;
        const float leadW = ImGui::CalcTextSize(name, label).x;

        ImVec2 pmin(0.0f, 0.0f);
        ImRect clip(0.0f, 0.0f, 0.0f, 0.0f);
        bool found = false;
        bool useClip = false;

        if (window->DockNode != nullptr && window->DockNode->TabBar != nullptr)
        {
            // Dock tabs are keyed by window->TabId (== GetID("#TAB")), not window->ID.
            ImGuiTabBar* tabBar = window->DockNode->TabBar;
            ImGuiTabItem* tab = ImGui::TabBarFindTabByID(tabBar, window->TabId);
            if (tab == nullptr)
            {
                for (int i = 0; i < tabBar->Tabs.Size; ++i)
                {
                    if (tabBar->Tabs[i].Window == window)
                    {
                        tab = &tabBar->Tabs[i];
                        break;
                    }
                }
            }
            if (tab != nullptr)
            {
                const float padX = ImGui::GetStyle().FramePadding.x;
                const float tabX = tabBar->BarRect.Min.x + tab->Offset - tabBar->ScrollingAnim;
                // Sit the icon immediately left of the visible label, UE-style.
                pmin.x = tabX + padX + leadW - iconSz - TabIconGap;
                pmin.y = tabBar->BarRect.Min.y + (tabBar->BarRect.GetHeight() - iconSz) * 0.5f;
                clip = tabBar->BarRect;
                useClip = true;
                found = true;
            }
        }
        else
        {
            const ImRect title = window->TitleBarRect();
            const float padX = ImGui::GetStyle().FramePadding.x;
            pmin.x = title.Min.x + window->WindowBorderSize + padX + leadW - iconSz - TabIconGap;
            pmin.y = title.Min.y + (title.GetHeight() - iconSz) * 0.5f;
            found = title.GetHeight() > 0.0f;
        }

        if (!found)
            return;

        // Dock tab / title chrome lives on the host window. The viewport
        // ForegroundDrawList is rendered after every window (including the
        // immersive overlay), so icons drawn there punch through F11 view.
        ImDrawList* dl = window->DrawList;
        if (window->DockNode != nullptr && window->DockNode->HostWindow != nullptr &&
            window->DockNode->HostWindow->DrawList != nullptr)
        {
            dl = window->DockNode->HostWindow->DrawList;
        }
        if (dl == nullptr)
            return;

        if (useClip)
            dl->PushClipRect(clip.Min, clip.Max, false);
        else
        {
            const ImRect title = window->TitleBarRect();
            dl->PushClipRect(title.Min, title.Max, false);
        }
        dl->AddImage(
            m_texIds[static_cast<UINT>(icon)],
            pmin,
            ImVec2(pmin.x + iconSz, pmin.y + iconSz));
        dl->PopClipRect();
    }
}
