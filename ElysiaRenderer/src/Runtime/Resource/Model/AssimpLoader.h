#pragma once

namespace ElysiaModel
{
    struct LoadedModel;
}

namespace ElysiaModel
{
    bool LoadModel(const wchar_t* filePath, bool bInvertTexcoordY, bool bImportMeshes,
            bool bImportSkeletons, bool bImportAnimations, float scale, LoadedModel &model);
}
