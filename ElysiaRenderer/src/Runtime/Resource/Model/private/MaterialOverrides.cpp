#include "stdafx.h"
#include "../public/MaterialOverrides.h"

#include "../public/LoadedModel.h"
#include "Programs/public/Hash.h"
#include "Programs/public/Log.h"
#include "Runtime/Resource/public/Serialization.h"

namespace ElysiaModel
{
    using namespace ElysiaHelper;

    namespace MaterialOverrides
    {
        static constexpr UINT32 kMagic = 0x54414D45; // 'EMAT'
        static constexpr UINT32 kVersion = 1;

        static std::wstring GetDirectory()
        {
            WCHAR assetsPath[512];
            GetAssetsPath(assetsPath, _countof(assetsPath));
            return std::wstring(assetsPath) + L"Saved\\Materials\\";
        }

        static std::wstring GetPath(const std::wstring& sourcePath)
        {
            wchar_t fileName[64];
            swprintf_s(fileName,
                       L"%016llx_v%u.emat",
                       static_cast<unsigned long long>(xxh::GetHash(sourcePath)),
                       kVersion);
            return GetDirectory() + fileName;
        }

        template <typename TSerializer>
        static void SerializeParameters(TSerializer& serializer, LoadedMaterial& material)
        {
            SerializeItem(serializer, material.albedoFactor);
            SerializeItem(serializer, material.opacity);
            SerializeItem(serializer, material.normalFactor);
            SerializeItem(serializer, material.metallicFactor);
            SerializeItem(serializer, material.roughnessFactor);
            SerializeItem(serializer, material.specularFactor);
            SerializeItem(serializer, material.alphaCutoff);
            INT32 shadingModelID = material.shadingModelID;
            SerializeItem(serializer, shadingModelID);
            if (TSerializer::IsReadSerializer())
                material.shadingModelID = shadingModelID;
            SerializeItem(serializer, material.emissiveFactor);
            SerializeItem(serializer, material.subsurfaceColor);
            SerializeItem(serializer, material.backLit);
        }

        static void SetDefaultBaseColor(LoadedModel& model)
        {
            for (auto& material : model.materials)
                material.albedoFactor = Vector3::One;
        }

        void Apply(LoadedModel& model)
        {
            SetDefaultBaseColor(model);
            if (model.sourcePath.empty() || model.materials.empty())
                return;

            const std::wstring path = GetPath(model.sourcePath);
            if (!FileExists(path))
                return;

            try
            {
                FileReadSerializer serializer(path);
                UINT32 magic = 0;
                UINT32 version = 0;
                UINT32 materialCount = 0;
                SerializeItem(serializer, magic);
                SerializeItem(serializer, version);
                SerializeItem(serializer, materialCount);
                if (magic != kMagic || version != kVersion)
                {
                    ElysiaHelper::Log::Warn("Material overrides: header mismatch for \"%s\".",
                              WstringToString(path).c_str());
                    return;
                }

                const UINT32 applyCount = std::min(materialCount,
                                                    static_cast<UINT32>(model.materials.size()));
                for (UINT32 i = 0; i < materialCount; ++i)
                {
                    if (i < applyCount)
                    {
                        SerializeParameters(serializer, model.materials[i]);
                    }
                    else
                    {
                        LoadedMaterial discarded;
                        SerializeParameters(serializer, discarded);
                    }
                }

                ElysiaHelper::Log::Info("Material overrides: loaded %u materials from \"%s\".",
                          applyCount,
                          WstringToString(path).c_str());
            }
            catch (...)
            {
                ElysiaHelper::Log::Warn("Material overrides: failed to read \"%s\".",
                          WstringToString(path).c_str());
                SetDefaultBaseColor(model);
            }
        }

        void SaveIfDirty(LoadedModel& model)
        {
            if (!model.materialParametersDirty || model.sourcePath.empty())
                return;

            const std::wstring directory = GetDirectory();
            std::error_code dirError;
            std::filesystem::create_directories(directory, dirError);
            if (dirError)
            {
                ElysiaHelper::Log::Warn("Material overrides: failed to create directory \"%s\".",
                          WstringToString(directory).c_str());
                return;
            }

            const std::wstring path = GetPath(model.sourcePath);
            const std::wstring tempPath = path + L".tmp";
            try
            {
                {
                    FileWriteSerializer serializer(tempPath);
                    UINT32 magic = kMagic;
                    UINT32 version = kVersion;
                    UINT32 materialCount = static_cast<UINT32>(model.materials.size());
                    SerializeItem(serializer, magic);
                    SerializeItem(serializer, version);
                    SerializeItem(serializer, materialCount);
                    for (UINT32 i = 0; i < materialCount; ++i)
                        SerializeParameters(serializer, model.materials[i]);
                }

                if (!MoveFileEx(tempPath.c_str(),
                                path.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                {
                    ElysiaHelper::Log::Warn("Material overrides: failed to commit \"%s\".",
                              WstringToString(path).c_str());
                    DeleteFile(tempPath.c_str());
                    return;
                }

                model.materialParametersDirty = false;
                ElysiaHelper::Log::Info("Material overrides: wrote \"%s\".", WstringToString(path).c_str());
            }
            catch (...)
            {
                ElysiaHelper::Log::Warn("Material overrides: failed to write \"%s\".",
                          WstringToString(path).c_str());
                DeleteFile(tempPath.c_str());
            }
        }
    }
}
