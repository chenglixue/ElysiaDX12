#include "stdafx.h"
#include "ModelCache.h"

#include "Programs/Log.h"
#include "Runtime/Resource/Serialization.h"
#include "Programs/Hash.h"

namespace ElysiaModel
{
    using namespace ElysiaHelper;

    namespace ModelCache
    {
        static constexpr UINT32 kMagic = 0x4C444D45; // 'EMDL' little-endian
        static constexpr UINT32 kVersion = 1;

        static UINT32 PackFlags(const ModelImportSettings& settings)
        {
            UINT32 flags = 0;
            if (settings.invertTexcoordY)
                flags |= 1u << 0;
            if (settings.importMeshes)
                flags |= 1u << 1;
            if (settings.importSkeletons)
                flags |= 1u << 2;
            if (settings.importAnimations)
                flags |= 1u << 3;
            return flags;
        }

        static UINT32 ScaleBits(float scale)
        {
            UINT32 bits = 0;
            memcpy(&bits, &scale, sizeof(bits));
            return bits;
        }

        static bool GetSourceStamp(const std::wstring& sourcePath, UINT64& writeTime, UINT64& fileSize)
        {
            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (!GetFileAttributesEx(sourcePath.c_str(), GetFileExInfoStandard, &attributes))
                return false;

            writeTime = attributes.ftLastWriteTime.dwLowDateTime |
                        (static_cast<UINT64>(attributes.ftLastWriteTime.dwHighDateTime) << 32);
            fileSize = attributes.nFileSizeLow |
                       (static_cast<UINT64>(attributes.nFileSizeHigh) << 32);
            return true;
        }

        static std::wstring GetCacheDirectory()
        {
            WCHAR assetsPath[512];
            GetAssetsPath(assetsPath, _countof(assetsPath));
            return std::wstring(assetsPath) + L"Cache\\Models\\";
        }

        static std::wstring GetCachePath(const std::wstring& sourcePath, const ModelImportSettings& settings)
        {
            const size_t pathHash = xxh::GetHash(sourcePath);
            const UINT32 scaleBits = ScaleBits(settings.scale);

            wchar_t fileName[64];
            swprintf_s(fileName,
                       L"%016llx_%08x_v%u.emdl",
                       static_cast<unsigned long long>(pathHash),
                       scaleBits,
                       kVersion);

            return GetCacheDirectory() + fileName;
        }

        template <typename TSerializer>
        static void SerializeHeader(TSerializer& serializer,
                                    UINT32& magic,
                                    UINT32& version,
                                    UINT32& flags,
                                    UINT32& scaleBits,
                                    UINT64& sourcePathHash,
                                    UINT64& sourceWriteTime,
                                    UINT64& sourceFileSize,
                                    UINT32& vertexStride,
                                    UINT32& indexStride)
        {
            SerializeItem(serializer, magic);
            SerializeItem(serializer, version);
            SerializeItem(serializer, flags);
            SerializeItem(serializer, scaleBits);
            SerializeItem(serializer, sourcePathHash);
            SerializeItem(serializer, sourceWriteTime);
            SerializeItem(serializer, sourceFileSize);
            SerializeItem(serializer, vertexStride);
            SerializeItem(serializer, indexStride);
        }

        bool TryLoad(const std::wstring& sourcePath,
                     const ModelImportSettings& settings,
                     LoadedModel& model)
        {
            const std::wstring cachePath = GetCachePath(sourcePath, settings);
            if (!FileExists(cachePath))
                return false;

            UINT64 sourceWriteTime = 0;
            UINT64 sourceFileSize = 0;
            if (!GetSourceStamp(sourcePath, sourceWriteTime, sourceFileSize))
                return false;

            try
            {
                bool headerValid = false;
                {
                    FileReadSerializer serializer(cachePath);

                    UINT32 magic = 0;
                    UINT32 version = 0;
                    UINT32 flags = 0;
                    UINT32 scaleBits = 0;
                    UINT64 sourcePathHash = 0;
                    UINT64 cachedWriteTime = 0;
                    UINT64 cachedFileSize = 0;
                    UINT32 vertexStride = 0;
                    UINT32 indexStride = 0;
                    SerializeHeader(serializer,
                                    magic,
                                    version,
                                    flags,
                                    scaleBits,
                                    sourcePathHash,
                                    cachedWriteTime,
                                    cachedFileSize,
                                    vertexStride,
                                    indexStride);

                    headerValid =
                        magic == kMagic &&
                        version == kVersion &&
                        flags == PackFlags(settings) &&
                        scaleBits == ScaleBits(settings.scale) &&
                        sourcePathHash == static_cast<UINT64>(xxh::GetHash(sourcePath)) &&
                        cachedWriteTime == sourceWriteTime &&
                        cachedFileSize == sourceFileSize &&
                        vertexStride == static_cast<UINT32>(sizeof(MeshVertex)) &&
                        indexStride == static_cast<UINT32>(sizeof(UINT32));

                    if (headerValid)
                    {
                        model.SerializeCPU(serializer);
                        ElysiaHelper::Log::Info("ModelCache: loaded \"%s\".", WstringToString(cachePath).c_str());
                        return true;
                    }
                }

                ElysiaHelper::Log::Warn("ModelCache: header mismatch for \"%s\", rebuilding.",
                          WstringToString(cachePath).c_str());
                DeleteFile(cachePath.c_str());
                return false;
            }
            catch (...)
            {
                ElysiaHelper::Log::Warn("ModelCache: failed to read \"%s\", rebuilding.",
                          WstringToString(cachePath).c_str());
                DeleteFile(cachePath.c_str());
                return false;
            }
        }

        bool TrySave(const std::wstring& sourcePath,
                     const ModelImportSettings& settings,
                     LoadedModel& model)
        {
            UINT64 sourceWriteTime = 0;
            UINT64 sourceFileSize = 0;
            if (!GetSourceStamp(sourcePath, sourceWriteTime, sourceFileSize))
                return false;

            const std::wstring cacheDir = GetCacheDirectory();
            std::error_code dirError;
            std::filesystem::create_directories(cacheDir, dirError);
            if (dirError)
            {
                ElysiaHelper::Log::Warn("ModelCache: failed to create directory \"%s\".",
                          WstringToString(cacheDir).c_str());
                return false;
            }

            const std::wstring cachePath = GetCachePath(sourcePath, settings);
            const std::wstring tempPath = cachePath + L".tmp";

            try
            {
                {
                    FileWriteSerializer serializer(tempPath);

                    UINT32 magic = kMagic;
                    UINT32 version = kVersion;
                    UINT32 flags = PackFlags(settings);
                    UINT32 scaleBits = ScaleBits(settings.scale);
                    UINT64 sourcePathHash = static_cast<UINT64>(xxh::GetHash(sourcePath));
                    UINT32 vertexStride = static_cast<UINT32>(sizeof(MeshVertex));
                    UINT32 indexStride = static_cast<UINT32>(sizeof(UINT32));
                    SerializeHeader(serializer,
                                    magic,
                                    version,
                                    flags,
                                    scaleBits,
                                    sourcePathHash,
                                    sourceWriteTime,
                                    sourceFileSize,
                                    vertexStride,
                                    indexStride);

                    model.SerializeCPU(serializer);
                }

                if (!MoveFileEx(tempPath.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                {
                    ElysiaHelper::Log::Warn("ModelCache: failed to commit \"%s\".", WstringToString(cachePath).c_str());
                    DeleteFile(tempPath.c_str());
                    return false;
                }

                ElysiaHelper::Log::Info("ModelCache: wrote \"%s\".", WstringToString(cachePath).c_str());
                return true;
            }
            catch (...)
            {
                ElysiaHelper::Log::Warn("ModelCache: failed to write \"%s\".", WstringToString(cachePath).c_str());
                DeleteFile(tempPath.c_str());
                return false;
            }
        }
    }
}
