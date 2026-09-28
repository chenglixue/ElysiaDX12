#pragma once

#include "LoadedModel.h"

namespace ElysiaModel
{
    struct ModelImportSettings
    {
        bool invertTexcoordY = true;
        bool importMeshes = true;
        bool importSkeletons = false;
        bool importAnimations = false;
        float scale = 1.0f;
    };

    namespace ModelCache
    {
        bool TryLoad(const std::wstring& sourcePath,
                     const ModelImportSettings& settings,
                     LoadedModel& model);

        bool TrySave(const std::wstring& sourcePath,
                     const ModelImportSettings& settings,
                     LoadedModel& model);
    }
}
