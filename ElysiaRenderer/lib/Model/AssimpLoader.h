#pragma once
#include "LoadedModel.h"

namespace ElysiaModel
{
    bool LoadModel(const wchar_t* filePath, bool mergeByMaterial, bool bInvertTexcoordY, bool bImportMeshes,
            bool bImportSkeletons, bool bImportAnimations, float scale, LoadedModel &model);
}
