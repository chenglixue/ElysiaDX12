#pragma once
#include "ThirdParty/imgui/imgui.h"

namespace ElysiaEditor
{
    // UE SWidgetDrawer Output Log: overlay above the status bar, not a docked tab.
    void DrawOutputLogDrawer(bool& open, const ImVec2& pos, const ImVec2& size, bool& hovered, float* heightDelta);
}