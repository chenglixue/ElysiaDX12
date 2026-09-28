#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ElysiaHelper
{
    struct LogEntry
    {
        uint8_t level = 0; // 与 Log::Level 的声明顺序一致：Info, Warn, Error
        uint32_t sequence = 0;
        uint64_t tickMs = 0;
        std::string text;
    };

    struct LogSnapshot
    {
        std::vector<LogEntry> entries;
        uint64_t dropped = 0;
    };

    class LogHistory
    {
    public:
        static void Push(uint8_t level, const std::string& text);
        static LogSnapshot Copy();
        static void Clear();
        static void ImportBuildDiagnostics();
    };
}