#pragma once
#include "src/Programs/Log.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    struct GIParameter
    {
        bool enableLine;
        float lineWidth;

        float normalBias;
        float viewBias;
        float blendWeight = 0.97f;
        float gamma = 5.f;
    };

    void DebugDumpTLASInstances(
        const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs,
        const std::vector<std::string>& instanceNames);
}