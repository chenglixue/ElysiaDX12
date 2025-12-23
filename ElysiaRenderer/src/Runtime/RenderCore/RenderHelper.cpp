#include "stdafx.h"
#include "RenderHelper.h"

namespace ElysiaHelper
{
    Vector4 GetZBufferParams(float nearZ, float farZ)
    {
        return Vector4(
            1 - farZ / nearZ,
            farZ / nearZ,
            (1 - farZ / nearZ) / farZ,
            (farZ / nearZ) / farZ);
    }
}