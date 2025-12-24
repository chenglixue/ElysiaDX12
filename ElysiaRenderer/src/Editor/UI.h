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
        
        bool  bUseMagnifier;

        
        void ToggleMagnifierLock();
        void ResetLPMSceneDefaults();

    };
}

