#pragma once

namespace ElysiaModel
{
    struct LoadedModel;
}

namespace ElysiaModel
{
    bool LoadModel(const std::wstring& filePath,
                   bool bInvertTexcoordY,
                   bool bImportMeshes,
                   bool bImportSkeletons,
                   bool bImportAnimations,
                   float scale,
                   LoadedModel& model);

    bool LoadGLTFModel(const std::wstring& filePath,
                       bool bInvertTexcoordY,
                       bool bImportMeshes,
                       bool bImportSkeletons,
                       bool bImportAnimations,
                       float scale,
                       LoadedModel& model);
}