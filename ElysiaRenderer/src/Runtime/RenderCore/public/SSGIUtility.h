#pragma once

namespace ElysiaRenderer
{
    // Basic standalone SSGI. Off by default. See Docs/UE5-SSGI.md for what
    // this covers and what it leaves out of the UE 5.7 path.
    struct SSGIParameter
    {
        bool enable = false;
        float intensity = 1.0f;
        // View-space length of the marched segment. UE's screen march runs to
        // the screen edge instead of a meter cutoff.
        float maxDistance = 20.0f;
        // Extra view-Z slack on top of one march step.
        float thickness = 1.0f;
        // Ignore crossings closer than this, so the ray does not hit itself.
        float selfBias = 0.05f;
        // How much of the previous indirect buffer to keep. 0 is this frame only.
        float historyWeight = 0.9f;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SSGIParameter,
                                                    enable,
                                                    intensity,
                                                    maxDistance,
                                                    thickness,
                                                    selfBias,
                                                    historyWeight)
}
