#pragma once

namespace ElysiaRenderer
{
    struct HairParameter
    {
        bool bEnableMultiScatter = true;
        bool bEnableR = true;
        bool bEnableTT = true;
        bool bEnableTRT = true;

        float backLit = 1.f;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HairParameter,
                                                    bEnableMultiScatter,
                                                    bEnableR,
                                                    bEnableTT,
                                                    bEnableTRT,
                                                    backLit
        )
}