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

    inline void ThrowRuntimeError(std::string output)
    {
        throw std::runtime_error(output);
    }
}