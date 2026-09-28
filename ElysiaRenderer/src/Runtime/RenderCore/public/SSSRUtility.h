#pragma once

namespace ElysiaRenderer
{
    struct SSSRParameter
    {
        float roughnessThreshold = 1;
        UINT samplesPerQuad = 1;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SSSRParameter,
                                                    roughnessThreshold,
                                                    samplesPerQuad)
}