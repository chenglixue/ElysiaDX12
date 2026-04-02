#pragma once
#include "ThirdParty/imgui/imgui.h"

namespace ElysiaRenderer
{
    template <typename EnumType>
    bool EnumCombo(const char* label, EnumType* current_value)
    {
        // 确保传入的类型是枚举
        static_assert(std::is_enum_v<EnumType>, "EnumType must be an enum.");

        bool value_changed = false;

        // 获取当前枚举的名称作为预览文本
        auto current_name = magic_enum::enum_name(*current_value);
        // magic_enum 返回的是 string_view，可能不以 '\0' 结尾，所以转为 std::string
        std::string preview_str(current_name);

        if (ImGui::BeginCombo(label, preview_str.c_str()))
        {
            // 获取所有枚举的值和名称
            constexpr auto entries = magic_enum::enum_entries<EnumType>();

            for (const auto& [value, name] : entries)
            {
                bool is_selected = (*current_value == value);
                std::string item_name(name);

                // 生成下拉菜单项
                if (ImGui::Selectable(item_name.c_str(), is_selected))
                {
                    *current_value = value;
                    value_changed = true;
                }

                // 如果当前项被选中，让滚动条默认聚焦在这一项
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        return value_changed;
    }
}