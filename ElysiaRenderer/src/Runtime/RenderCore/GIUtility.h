#pragma once
#include "src/Programs/Log.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    struct GIParameter
    {
        bool enableLine;
        bool bHideInactiveProbe;
        bool bTextureVisualization;
        float lineWidth;

        float normalBias;
        float viewBias;
        float blendWeight = 0.97f;
        float gamma = 5.f;
        float probeIrradianceThreshold;
        float probeBrightnessThreshold;
        Vector3 probeGroupOrigin = Vector3(0, 0, 0);
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GIParameter,
                                                    enableLine,
                                                    lineWidth,
                                                    normalBias,
                                                    viewBias,
                                                    blendWeight,
                                                    gamma,
                                                    probeIrradianceThreshold,
                                                    probeBrightnessThreshold,
                                                    probeGroupOrigin
        )

    void DebugDumpTLASInstances(
        const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs,
        const std::vector<std::string>& instanceNames);
}