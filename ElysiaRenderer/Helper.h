#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include "SimpleMath/SimpleMath.h"

namespace ElysiaHelper
{
    inline void AssertIfFailed(HRESULT hr)
    {
        assert(SUCCEEDED(hr));
    }

    inline void AssertError(const char* errorMessage)
    {
        assert((errorMessage, false));
    }

    inline void ThrowRuntimeError(std::string output)
    {
        throw std::runtime_error(output);
    }

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch DirectX API errors
            throw std::exception();
        }
    }

    inline uint32_t AlignU32(uint32_t valueToAlign, uint32_t alignment)
    {
        alignment -= 1;
        return (uint32_t)((valueToAlign + alignment) & ~alignment);
    }

    inline uint64_t AlignU64(uint64_t valueToAlign, uint64_t alignment)
    {
        alignment -= 1;
        return (uint64_t)((valueToAlign + alignment) & ~alignment);
    }

    template <class T> void SafeRelease(T& ppT)
    {
        if (ppT)
        {
            ppT->Release();
            ppT = nullptr;
        }
    }

    struct UINT2
    {
        uint32_t x = 0;
        uint32_t y = 0;
    };
}