#include "stdafx.h"
#include "../public/LogHistory.h"

#include <cctype>
#include <deque>
#include <fstream>

namespace ElysiaHelper
{
    constexpr size_t kCapacity = 4096;

    struct State
    {
        std::mutex mutex;
        std::deque<LogEntry> entries;
        uint32_t sequence = 0;
        uint64_t dropped = 0;
        uint64_t startTick = GetTickCount64();
    };

    State& GetState()
    {
        static State state;
        return state;
    }

    void LogHistory::Push(uint8_t level, const std::string& text)
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.entries.size() >= kCapacity)
        {
            state.entries.pop_front();
            ++state.dropped;
        }

        LogEntry entry;
        entry.level = level;
        entry.sequence = ++state.sequence;
        entry.tickMs = GetTickCount64() - state.startTick;
        entry.text = text;
        state.entries.push_back(std::move(entry));
    }

    LogSnapshot LogHistory::Copy()
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        LogSnapshot snapshot;
        snapshot.dropped = state.dropped;
        snapshot.entries.assign(state.entries.begin(), state.entries.end());
        return snapshot;
    }

    void LogHistory::Clear()
    {
        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.entries.clear();
        state.dropped = 0;
    }

    namespace
    {
        std::string Trim(const std::string& line)
        {
            size_t begin = 0;
            while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t' || line[begin] == '\r'))
                ++begin;
            size_t end = line.size();
            while (end > begin && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r'))
                --end;
            return line.substr(begin, end - begin);
        }

        std::string Lower(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            return text;
        }

        uint8_t ClassifyBuildLine(const std::string& line)
        {
            const std::string lower = Lower(line);
            if (lower.find("fatal error") != std::string::npos ||
                lower.find(": error") != std::string::npos ||
                lower.find("error lnk") != std::string::npos)
                return 2;
            if (lower.find(": warning") != std::string::npos ||
                lower.find(" warning ") != std::string::npos ||
                lower.find("warning lnk") != std::string::npos)
                return 1;
            if (lower.find(": message") != std::string::npos)
                return 0;
            return 255;
        }

        void ConsiderLog(const std::filesystem::path& path,
                         std::filesystem::path& best,
                         std::filesystem::file_time_type& bestTime,
                         bool& found)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
                return;
            const auto time = std::filesystem::last_write_time(path, ec);
            if (ec)
                return;
            if (!found || time > bestTime)
            {
                best = path;
                bestTime = time;
                found = true;
            }
        }
    }

    void LogHistory::ImportBuildDiagnostics()
    {
        std::vector<std::filesystem::path> roots;
        roots.push_back(std::filesystem::current_path());

        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::filesystem::path dir = std::filesystem::path(exePath).parent_path();
        for (int i = 0; i < 6 && !dir.empty(); ++i)
        {
            roots.push_back(dir);
            dir = dir.parent_path();
        }

        std::filesystem::path best;
        std::filesystem::file_time_type bestTime{};
        bool found = false;
        for (const std::filesystem::path& root : roots)
        {
            ConsiderLog(root / "ElysiaRenderer.log", best, bestTime, found);
            ConsiderLog(root / "x64" / "Debug" / "ElysiaRenderer.log", best, bestTime, found);
            ConsiderLog(root / "x64" / "Release" / "ElysiaRenderer.log", best, bestTime, found);
            ConsiderLog(root / "ElysiaRenderer" / "x64" / "Debug" / "ElysiaRenderer.log", best, bestTime, found);
            ConsiderLog(root / "ElysiaRenderer" / "x64" / "Release" / "ElysiaRenderer.log", best, bestTime, found);
        }
        if (!found)
            return;

        std::ifstream input(best, std::ios::binary);
        if (!input)
            return;

        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && static_cast<unsigned char>(line[0]) == 0xEF)
                line.erase(0, line.size() >= 3 ? 3 : 0);
            const std::string text = Trim(line);
            const uint8_t level = ClassifyBuildLine(text);
            if (level == 255)
                continue;
            Push(level, text);
        }
    }
}