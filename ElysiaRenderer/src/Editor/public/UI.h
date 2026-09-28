#pragma once

namespace ElysiaEditor
{
    struct UIState
    {
        //
        // WINDOW MANAGEMENT
        //
        bool bShowControlsWindow;
        bool bShowProfilerWindow;

        bool bUseMagnifier;

        bool bShowOutputLog = true;

        void ToggleMagnifierLock();
        void ResetLPMSceneDefaults();

    };
}