#include "stdafx.h"
#include "../public/ShaderBytecodeCache.h"

#include "Programs/public/Hash.h"
#include "Programs/public/Helper.h"
#include "Programs/public/Log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace ElysiaCore
{
    namespace
    {
        constexpr char kCacheMagic[8] = {'E', 'S', 'H', 'A', 'D', 'E', 'R', 'C'};
        constexpr uint32_t kCacheVersion = 1;
        constexpr size_t kMaxBlobBytes = 64u * 1024u * 1024u;
        constexpr size_t kMaxIncludeBytes = 16u * 1024u * 1024u;
        constexpr int kMaxIncludeDepth = 32;

        struct BytecodeKey
        {
            uint64_t source;
            uint64_t includes;
            uint64_t arguments;
            uint64_t compiler;
            uint32_t version;
            uint32_t pad;
        };

        struct CacheEntry
        {
            std::vector<uint8_t> object;
            std::vector<uint8_t> reflection;
        };

        std::mutex g_mutex;
        bool g_loaded = false;
        std::unordered_map<uint64_t, CacheEntry> g_entries;

        std::filesystem::path CacheFilePath()
        {
            WCHAR assetsPath[512] = {};
            ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
            return std::filesystem::path(assetsPath) / L"Saved" / L"ShaderCache" / L"D3D12.dxil";
        }

        std::string Narrow(const std::wstring& text)
        {
            if (text.empty())
                return {};
            const int length = WideCharToMultiByte(CP_UTF8,
                                                   0,
                                                   text.c_str(),
                                                   -1,
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   nullptr);
            if (length <= 1)
                return {};
            std::string narrow(static_cast<size_t>(length - 1), '\0');
            WideCharToMultiByte(CP_UTF8,
                                0,
                                text.c_str(),
                                -1,
                                narrow.data(),
                                length,
                                nullptr,
                                nullptr);
            return narrow;
        }

        uint64_t Mix(uint64_t seed, const void* data, size_t size)
        {
            if (data == nullptr || size == 0)
                return seed;
            const uint64_t part = static_cast<uint64_t>(xxh::GetHash(data, size));
            const uint64_t pair[2] = {seed, part};
            return static_cast<uint64_t>(xxh::GetHash(pair, sizeof(pair)));
        }

        uint64_t CompilerStamp()
        {
            static const uint64_t stamp = []()
            {
                wchar_t modulePath[MAX_PATH] = {};
                const HMODULE module = GetModuleHandleW(L"dxcompiler.dll");
                if (module == nullptr || GetModuleFileNameW(module, modulePath, MAX_PATH) == 0)
                {
                    ElysiaHelper::GetAssetsPath(modulePath, MAX_PATH);
                    wcscat_s(modulePath, L"dxcompiler.dll");
                }
                std::error_code error;
                const auto fileSize = std::filesystem::file_size(modulePath, error);
                const auto writeTime = std::filesystem::last_write_time(modulePath, error);
                uint64_t parts[2] = {};
                parts[0] = error ? 0 : static_cast<uint64_t>(fileSize);
                parts[1] = error
                               ? 0
                               : static_cast<uint64_t>(writeTime.time_since_epoch().count());
                return static_cast<uint64_t>(xxh::GetHash(parts, sizeof(parts)));
            }();
            return stamp;
        }

        std::wstring DecodeText(const void* data, size_t size)
        {
            if (data == nullptr || size == 0)
                return {};
            const auto* bytes = static_cast<const unsigned char*>(data);
            if (size >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
            {
                return std::wstring(reinterpret_cast<const wchar_t*>(bytes + 2), (size - 2) / sizeof(wchar_t));
            }

            int wideLength = MultiByteToWideChar(CP_UTF8,
                                                 MB_ERR_INVALID_CHARS,
                                                 reinterpret_cast<const char*>(bytes),
                                                 static_cast<int>(size),
                                                 nullptr,
                                                 0);
            UINT codePage = CP_UTF8;
            if (wideLength <= 0)
            {
                codePage = CP_ACP;
                wideLength = MultiByteToWideChar(codePage,
                                                 0,
                                                 reinterpret_cast<const char*>(bytes),
                                                 static_cast<int>(size),
                                                 nullptr,
                                                 0);
            }
            if (wideLength <= 0)
                return {};
            std::wstring text(static_cast<size_t>(wideLength), L'\0');
            MultiByteToWideChar(codePage,
                                0,
                                reinterpret_cast<const char*>(bytes),
                                static_cast<int>(size),
                                text.data(),
                                wideLength);
            return text;
        }

        std::wstring Trim(std::wstring_view line)
        {
            size_t begin = 0;
            while (begin < line.size() && iswspace(line[begin]))
                ++begin;
            size_t end = line.size();
            while (end > begin && iswspace(line[end - 1]))
                --end;
            return std::wstring(line.substr(begin, end - begin));
        }

        bool ParseInclude(const std::wstring& line, bool& quoted, std::wstring& specifier)
        {
            const size_t comment = line.find(L"//");
            const size_t marker = line.find(L"#");
            if (marker == std::wstring::npos || (comment != std::wstring::npos && comment < marker))
                return false;
            size_t cursor = marker + 1;
            while (cursor < line.size() && iswspace(line[cursor]))
                ++cursor;
            if (line.compare(cursor, 7, L"include") != 0)
                return false;
            cursor += 7;
            while (cursor < line.size() && iswspace(line[cursor]))
                ++cursor;
            if (cursor >= line.size())
                return false;
            const wchar_t open = line[cursor];
            const wchar_t close = open == L'<' ? L'>' : L'"';
            if (open != L'<' && open != L'"')
                return false;
            const size_t finish = line.find(close, cursor + 1);
            if (finish == std::wstring::npos)
                return false;
            quoted = open == L'"';
            specifier = line.substr(cursor + 1, finish - cursor - 1);
            return !specifier.empty();
        }

        std::filesystem::path ResolveInclude(const std::wstring& specifier,
                                             bool quoted,
                                             const std::filesystem::path& currentDirectory,
                                             const std::vector<std::filesystem::path>& includeDirs)
        {
            std::vector<std::filesystem::path> candidates;
            if (quoted)
                candidates.push_back(currentDirectory / specifier);
            for (const auto& directory : includeDirs)
                candidates.push_back(directory / specifier);

            for (const auto& candidate : candidates)
            {
                std::error_code error;
                if (!std::filesystem::is_regular_file(candidate, error))
                    continue;
                std::filesystem::path canonical = std::filesystem::weakly_canonical(candidate, error);
                if (error)
                    canonical = candidate;
                return canonical;
            }
            return {};
        }

        void VisitIncludes(const std::wstring& text,
                           const std::filesystem::path& currentDirectory,
                           const std::vector<std::filesystem::path>& includeDirs,
                           std::unordered_set<std::wstring>& visited,
                           std::vector<std::pair<std::wstring, uint64_t>>& includes,
                           int depth)
        {
            if (depth > kMaxIncludeDepth)
                return;

            size_t lineStart = 0;
            while (lineStart < text.size())
            {
                size_t lineEnd = text.find(L'\n', lineStart);
                if (lineEnd == std::wstring::npos)
                    lineEnd = text.size();
                bool quoted = false;
                std::wstring specifier;
                if (ParseInclude(Trim(text.substr(lineStart, lineEnd - lineStart)), quoted, specifier))
                {
                    const std::filesystem::path resolved = ResolveInclude(specifier,
                                                                          quoted,
                                                                          currentDirectory,
                                                                          includeDirs);
                    const std::wstring key = resolved.empty()
                                                ? specifier
                                                : resolved.generic_wstring();
                    if (visited.insert(key).second)
                    {
                        uint64_t content = 0;
                        std::wstring nested;
                        if (!resolved.empty())
                        {
                            std::ifstream file(resolved, std::ios::binary);
                            if (file)
                            {
                                file.seekg(0, std::ios::end);
                                const auto length = file.tellg();
                                file.seekg(0, std::ios::beg);
                                if (length > 0 && static_cast<size_t>(length) <= kMaxIncludeBytes)
                                {
                                    std::vector<char> bytes(static_cast<size_t>(length));
                                    file.read(bytes.data(), length);
                                    if (file)
                                    {
                                        content = static_cast<uint64_t>(xxh::GetHash(bytes.data(), bytes.size()));
                                        nested = DecodeText(bytes.data(), bytes.size());
                                    }
                                }
                            }
                        }
                        includes.emplace_back(key, content);
                        if (!nested.empty())
                        {
                            VisitIncludes(nested,
                                          resolved.parent_path(),
                                          includeDirs,
                                          visited,
                                          includes,
                                          depth + 1);
                        }
                    }
                }
                if (lineEnd == text.size())
                    break;
                lineStart = lineEnd + 1;
            }
        }

        uint64_t HashIncludes(const ShaderBytecodeLookup& lookup)
        {
            std::vector<std::filesystem::path> includeDirs;
            if (lookup.arguments != nullptr)
            {
                for (const std::wstring& argument : *lookup.arguments)
                {
                    if (argument.rfind(L"-I", 0) == 0 && argument.size() > 2)
                        includeDirs.emplace_back(argument.substr(2));
                }
            }

            WCHAR assetsPath[512] = {};
            ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
            const std::filesystem::path mainFile = std::filesystem::path(assetsPath) / lookup.shaderName;
            std::unordered_set<std::wstring> visited;
            std::vector<std::pair<std::wstring, uint64_t>> includes;
            VisitIncludes(DecodeText(lookup.source, lookup.sourceSize),
                          mainFile.parent_path(),
                          includeDirs,
                          visited,
                          includes,
                          0);
            std::sort(includes.begin(), includes.end(), [](const auto& left, const auto& right)
            {
                return left.first < right.first;
            });

            uint64_t hash = 0;
            for (const auto& include : includes)
            {
                hash = Mix(hash, include.first.data(), include.first.size() * sizeof(wchar_t));
                hash = Mix(hash, &include.second, sizeof(include.second));
            }
            return hash;
        }

        uint64_t HashArguments(const ShaderBytecodeLookup& lookup)
        {
            uint64_t hash = 0;
            if (lookup.arguments == nullptr)
                return hash;
            for (const std::wstring& argument : *lookup.arguments)
            {
                if (argument.rfind(L"-I", 0) == 0)
                    continue;
                hash = Mix(hash, argument.data(), argument.size() * sizeof(wchar_t));
            }
            return hash;
        }

        uint64_t MakeKey(const ShaderBytecodeLookup& lookup)
        {
            BytecodeKey key{};
            key.source = lookup.source == nullptr
                             ? 0
                             : static_cast<uint64_t>(xxh::GetHash(lookup.source, lookup.sourceSize));
            key.includes = HashIncludes(lookup);
            key.arguments = HashArguments(lookup);
            key.compiler = CompilerStamp();
            key.version = kCacheVersion;
            key.pad = 0;
            return static_cast<uint64_t>(xxh::GetHash(key));
        }

        std::string Describe(const ShaderBytecodeLookup& lookup)
        {
            std::wstring entry;
            std::wstring target;
            if (lookup.arguments != nullptr)
            {
                const auto& arguments = *lookup.arguments;
                for (size_t i = 0; i + 1 < arguments.size(); ++i)
                {
                    if (arguments[i] == L"-E")
                        entry = arguments[i + 1];
                    else if (arguments[i] == L"-T")
                        target = arguments[i + 1];
                }
            }
            std::string text = Narrow(std::wstring(lookup.shaderName));
            if (!entry.empty())
                text += " " + Narrow(entry);
            if (!target.empty())
                text += " " + Narrow(target);
            return text;
        }

        void LoadCache()
        {
            if (g_loaded)
                return;
            g_loaded = true;

            const std::filesystem::path path = CacheFilePath();
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return;

            char magic[8] = {};
            uint32_t version = 0;
            uint32_t count = 0;
            file.read(magic, sizeof(magic));
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            file.read(reinterpret_cast<char*>(&count), sizeof(count));
            if (!file || std::memcmp(magic, kCacheMagic, sizeof(magic)) != 0 || version != kCacheVersion)
            {
                ElysiaHelper::Log::Warn("Shader bytecode cache: ignored unreadable cache %s",
                                        path.string().c_str());
                return;
            }

            for (uint32_t index = 0; index < count; ++index)
            {
                uint64_t key = 0;
                uint32_t objectSize = 0;
                uint32_t reflectionSize = 0;
                file.read(reinterpret_cast<char*>(&key), sizeof(key));
                file.read(reinterpret_cast<char*>(&objectSize), sizeof(objectSize));
                file.read(reinterpret_cast<char*>(&reflectionSize), sizeof(reflectionSize));
                if (!file || objectSize == 0 || objectSize > kMaxBlobBytes || reflectionSize > kMaxBlobBytes)
                {
                    g_entries.clear();
                    ElysiaHelper::Log::Warn("Shader bytecode cache: ignored truncated cache %s",
                                            path.string().c_str());
                    return;
                }
                CacheEntry entry;
                entry.object.resize(objectSize);
                entry.reflection.resize(reflectionSize);
                file.read(reinterpret_cast<char*>(entry.object.data()), objectSize);
                if (reflectionSize > 0)
                    file.read(reinterpret_cast<char*>(entry.reflection.data()), reflectionSize);
                if (!file)
                {
                    g_entries.clear();
                    ElysiaHelper::Log::Warn("Shader bytecode cache: ignored truncated cache %s",
                                            path.string().c_str());
                    return;
                }
                g_entries.emplace(key, std::move(entry));
            }

            ElysiaHelper::Log::Info("Shader bytecode cache: %u entries from %s",
                                    static_cast<unsigned>(g_entries.size()),
                                    path.string().c_str());
        }

        void SaveCache()
        {
            const std::filesystem::path path = CacheFilePath();
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            const std::filesystem::path temporary = path.string() + ".tmp";
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                if (!file)
                {
                    ElysiaHelper::Log::Warn("Shader bytecode cache: failed to write %s",
                                            temporary.string().c_str());
                    return;
                }
                const uint32_t version = kCacheVersion;
                const uint32_t count = static_cast<uint32_t>(g_entries.size());
                file.write(kCacheMagic, sizeof(kCacheMagic));
                file.write(reinterpret_cast<const char*>(&version), sizeof(version));
                file.write(reinterpret_cast<const char*>(&count), sizeof(count));
                for (const auto& item : g_entries)
                {
                    const uint32_t objectSize = static_cast<uint32_t>(item.second.object.size());
                    const uint32_t reflectionSize = static_cast<uint32_t>(item.second.reflection.size());
                    file.write(reinterpret_cast<const char*>(&item.first), sizeof(item.first));
                    file.write(reinterpret_cast<const char*>(&objectSize), sizeof(objectSize));
                    file.write(reinterpret_cast<const char*>(&reflectionSize), sizeof(reflectionSize));
                    file.write(reinterpret_cast<const char*>(item.second.object.data()), objectSize);
                    if (reflectionSize > 0)
                    {
                        file.write(reinterpret_cast<const char*>(item.second.reflection.data()),
                                   reflectionSize);
                    }
                }
                if (!file)
                {
                    ElysiaHelper::Log::Warn("Shader bytecode cache: failed to write %s",
                                            temporary.string().c_str());
                    return;
                }
            }
            std::filesystem::remove(path, error);
            std::filesystem::rename(temporary, path, error);
            if (error)
            {
                ElysiaHelper::Log::Warn("Shader bytecode cache: failed to replace %s",
                                        path.string().c_str());
            }
        }
    }

    ShaderBytecodeCache& ShaderBytecodeCache::Get()
    {
        static ShaderBytecodeCache instance;
        return instance;
    }

    bool ShaderBytecodeCache::TryGet(const ShaderBytecodeLookup& lookup,
                                     std::vector<uint8_t>& object,
                                     std::vector<uint8_t>& reflection)
    {
        const uint64_t key = MakeKey(lookup);
        std::lock_guard lock(g_mutex);
        LoadCache();
        const auto it = g_entries.find(key);
        if (it == g_entries.end())
            return false;
        object = it->second.object;
        reflection = it->second.reflection;
        ElysiaHelper::Log::Info("Shader bytecode cache hit: %s", Describe(lookup).c_str());
        return true;
    }

    void ShaderBytecodeCache::Put(const ShaderBytecodeLookup& lookup,
                                  const void* object,
                                  size_t objectSize,
                                  const void* reflection,
                                  size_t reflectionSize)
    {
        if (object == nullptr || objectSize == 0 || objectSize > kMaxBlobBytes ||
            reflectionSize > kMaxBlobBytes)
            return;

        const uint64_t key = MakeKey(lookup);
        {
            std::lock_guard lock(g_mutex);
            LoadCache();
            CacheEntry entry;
            entry.object.assign(static_cast<const uint8_t*>(object),
                                static_cast<const uint8_t*>(object) + objectSize);
            if (reflection != nullptr && reflectionSize > 0)
            {
                entry.reflection.assign(static_cast<const uint8_t*>(reflection),
                                        static_cast<const uint8_t*>(reflection) + reflectionSize);
            }
            g_entries[key] = std::move(entry);
            SaveCache();
        }
        ElysiaHelper::Log::Info("Shader bytecode compiled: %s", Describe(lookup).c_str());
    }
}
