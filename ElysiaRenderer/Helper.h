#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
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
}