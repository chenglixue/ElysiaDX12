#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ElysiaCore
{
    // Disk cache of DXIL plus the DXC reflection blob. The key is the source
    // buffer, the included files, the compiler arguments, and dxcompiler.dll.
    // Include paths are not part of the key. A changed HLSL or hlsl include
    // misses and is compiled again.
    struct ShaderBytecodeLookup
    {
        const void* source = nullptr;
        size_t sourceSize = 0;
        std::wstring_view shaderName;
        const std::vector<std::wstring>* arguments = nullptr;
    };

    class ShaderBytecodeCache
    {
    public:
        static ShaderBytecodeCache& Get();

        bool TryGet(const ShaderBytecodeLookup& lookup,
                    std::vector<uint8_t>& object,
                    std::vector<uint8_t>& reflection);
        void Put(const ShaderBytecodeLookup& lookup,
                 const void* object,
                 size_t objectSize,
                 const void* reflection,
                 size_t reflectionSize);

    private:
        ShaderBytecodeCache() = default;
    };
}
