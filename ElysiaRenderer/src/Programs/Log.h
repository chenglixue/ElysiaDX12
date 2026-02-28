#pragma once

namespace ElysiaHelper
{
    template <typename T, typename... Args>
    void Log(T&& first, Args&&... args)
    {
        std::stringstream ss;
        ss << std::forward<T>(first);

        // 展开剩余参数
        ((ss << std::forward<Args>(args)), ...);

        std::string s = "[Elysia] " + ss.str() + "\n";
        OutputDebugStringA(s.c_str());
        printf("%s", s.c_str());
    }

    class Log
    {
    public:
        enum class Level
        {
            Info,
            Warn,
            Error
        };

        template <typename... Args>
        static void Info(const char* format, Args&&... args)
        {
            FormatAndOutput(Level::Info, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Warn(const char* format, Args&&... args)
        {
            FormatAndOutput(Level::Warn, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Error(const char* format, Args&&... args)
        {
            FormatAndOutput(Level::Error, format, std::forward<Args>(args)...);
        }

    private:
        template <typename... Args>
        static void FormatAndOutput(Level level, const char* format, Args&&... args)
        {
            // 1. 计算所需缓冲区长度 (snprintf 会处理参数包展开)
            int size = snprintf(nullptr, 0, format, std::forward<Args>(args)...);
            if (size <= 0)
                return;

            // 2. 分配内存并格式化
            std::string buf(size, '\0');
            snprintf(&buf[0], size + 1, format, std::forward<Args>(args)...);

            // 3. 最终输出
            Output(level, buf);
        }

        // 最终的非模板输出函数，彻底杜绝递归
        static void Output(Level level, const std::string& message)
        {
            const char* colorCode = "";
            const char* levelStr = "";

            switch (level)
            {
            case Level::Info:
                colorCode = "\033[37m";
                levelStr = "[INFO]";
                break;
            case Level::Warn:
                colorCode = "\033[33m";
                levelStr = "[WARN]";
                break;
            case Level::Error:
                colorCode = "\033[31m";
                levelStr = "[ERROR]";
                break;
            }

            std::string finalMessage = "[Elysia]" + std::string(levelStr) + " " + message + "\n";

            // 输出到 Visual Studio 调试器 [cite: 2025-12-29]
            OutputDebugStringA(finalMessage.c_str());

            // 输出到控制台（带 ANSI 颜色）
            printf("%s%s\033[0m", colorCode, finalMessage.c_str());
        }
    };
}