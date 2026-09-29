#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ElysiaRenderer
{
    // Layered ini cache. Base, then Default, then Saved, then the command line.
    // Saved stores only keys that differ from the first two layers.
    class ConfigCache
    {
    public:
        static ConfigCache& Get();

        void LoadHierarchy();
        void Apply();
        void SaveDiff() const;

    private:
        ConfigCache() = default;

        std::unordered_map<std::string, std::string> m_scalars;
        std::unordered_map<std::string, std::vector<std::string>> m_arrays;
        std::unordered_map<std::string, std::string> m_baselineScalars;
        std::unordered_map<std::string, std::vector<std::string>> m_baselineArrays;

        bool TryGet(const char* section, const char* key, std::string& out) const;
        bool TryGetBaseline(const char* section, const char* key, std::string& out) const;
        const std::vector<std::string>* TryGetArray(const char* section, const char* key) const;
        const std::vector<std::string>* TryGetBaselineArray(const char* section, const char* key) const;

        void LoadFile(const std::filesystem::path& path, bool required);
        void LoadCommandLine();
        void Ingest(const std::string& line,
                    std::string& section,
                    std::unordered_set<std::string>& dotted);
    };
}
