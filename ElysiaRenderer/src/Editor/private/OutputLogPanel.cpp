#include "stdafx.h"
#include "../public/OutputLogPanel.h"

#include "Programs/public/LogHistory.h"
#include "ThirdParty/imgui/imgui.h"

namespace ElysiaEditor
{
    struct ViewRow
    {
        uint8_t level;
        int count;
        const std::string* text;
        uint64_t tickMs;
    };

    ImVec4 LevelColor(uint8_t level)
    {
        if (level == 1)
            return ImVec4(1.0f, 0.85f, 0.35f, 1.0f);
        if (level == 2)
            return ImVec4(1.0f, 0.45f, 0.40f, 1.0f);
        return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    }

    const char* LevelLabel(uint8_t level)
    {
        if (level == 1)
            return "Warn";
        if (level == 2)
            return "Error";
        return "Info";
    }

    void DrawOutputLog(bool& open)
    {
        if (!ImGui::Begin("Output Log", &open))
        {
            ImGui::End();
            return;
        }

        static bool showInfo = true;
        static bool showWarn = true;
        static bool showError = true;
        static bool collapse = true;
        static bool stickToBottom = true;
        static ImGuiTextFilter filter;

        if (ImGui::Button("Clear"))
            ElysiaHelper::LogHistory::Clear();
        ImGui::SameLine();
        ImGui::Checkbox("Info", &showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &showError);
        ImGui::SameLine();
        ImGui::Checkbox("Collapse", &collapse);
        ImGui::SameLine();
        filter.Draw("##filter", 180.0f);

        const ElysiaHelper::LogSnapshot snapshot = ElysiaHelper::LogHistory::Copy();
        if (snapshot.dropped > 0)
            ImGui::TextDisabled("dropped %llu", static_cast<unsigned long long>(snapshot.dropped));

        std::vector<ViewRow> rows;
        rows.reserve(snapshot.entries.size());
        for (const ElysiaHelper::LogEntry& entry : snapshot.entries)
        {
            const bool visible =
                (entry.level == 0 && showInfo) ||
                (entry.level == 1 && showWarn) ||
                (entry.level == 2 && showError);
            if (!visible)
                continue;
            if (!filter.PassFilter(entry.text.c_str()))
                continue;

            if (collapse && !rows.empty() &&
                rows.back().level == entry.level &&
                *rows.back().text == entry.text)
            {
                ++rows.back().count;
                continue;
            }

            rows.push_back({entry.level, 1, &entry.text, entry.tickMs});
        }

        if (ImGui::Button("Copy"))
        {
            std::string text;
            for (const ViewRow& row : rows)
            {
                text += LevelLabel(row.level);
                text += "  ";
                text += *row.text;
                if (row.count > 1)
                    text += "  x" + std::to_string(row.count);
                text += "\n";
            }
            ImGui::SetClipboardText(text.c_str());
        }

        ImGui::Separator();
        ImGui::BeginChild("##logscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const ViewRow& row = rows[i];
                const unsigned sec = static_cast<unsigned>(row.tickMs / 1000);
                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(row.level));
                if (row.count > 1)
                {
                    ImGui::Text("[%02u:%02u] %s  %s  x%d",
                                sec / 60,
                                sec % 60,
                                LevelLabel(row.level),
                                row.text->c_str(),
                                row.count);
                }
                else
                {
                    ImGui::Text("[%02u:%02u] %s  %s",
                                sec / 60,
                                sec % 60,
                                LevelLabel(row.level),
                                row.text->c_str());
                }
                ImGui::PopStyleColor();
            }
        }

        if (stickToBottom && atBottom)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
}