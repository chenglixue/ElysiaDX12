#pragma once

namespace ElysiaRenderer
{
    struct SubsurfaceScatterParameter
    {
        float CurveScale = 1.f;
        float MinCurve = 0.2f;
        Vector3 SubsurfaceColor = Vector3::One;
        float ScatterRadius = 1.f;
        float TransmissionScale = 1.f;
        float TransmissionRange = 1.f;
        float TransmissionEdgeGlow = 1.f;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SubsurfaceScatterParameter,
                                                    CurveScale,
                                                    MinCurve,
                                                    SubsurfaceColor,
                                                    ScatterRadius,
                                                    TransmissionScale,
                                                    TransmissionRange,
                                                    TransmissionEdgeGlow)
}