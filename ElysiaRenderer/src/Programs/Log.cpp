#include "stdafx.h"
#include "Log.h"

namespace ElysiaHelper
{
    void Log(const char* format, ...)
    {
        char buffer[1024]; // 预设一个足够大的缓冲区
        va_list args;
        va_start(args, format);

        // 使用 vsnprintf 安全地处理变长参数
        int result = vsnprintf(buffer, sizeof(buffer), format, args);

        va_end(args);

        if (result > 0)
        {
            // 1. 输出到 Visual Studio 调试输出窗口
            OutputDebugStringA(buffer);
            OutputDebugStringA("\n");

            // 2. 同时输出到标准控制台（如果开启了控制台）
            printf("%s\n", buffer);
        }
    }
}