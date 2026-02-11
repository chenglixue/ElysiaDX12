#pragma once
#include "src/Programs/Log.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    struct GIDebugParameter
    {
        bool enableLine;
        float lineWidth;
    };

    void DebugDumpTLASInstances(
        const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs,
        const std::vector<std::string>& instanceNames);
}