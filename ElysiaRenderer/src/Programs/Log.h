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
}