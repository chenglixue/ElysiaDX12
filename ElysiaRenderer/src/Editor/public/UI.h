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

        // UE StatusBar Output Log drawer: closed until the status-bar button is clicked.
        bool bShowOutputLog = false;
        float outputLogDrawerHeight = 0.0f;

        void ToggleMagnifierLock();
        void ResetLPMSceneDefaults();

    };
}