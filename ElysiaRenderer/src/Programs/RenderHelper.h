#pragma once
#include "Programs/Helper.h"

namespace ElysiaHelper
{
    Vector4 GetZBufferParams(float nearZ, float farZ);
    
    void SetViewportAndScissor(ID3D12GraphicsCommandList* pCommandList, uint32_t topLeftX, uint32_t topLeftY, uint32_t width, uint32_t height);

    void SetName(ID3D12Object *pObj, const char * name);

    void SetName(ID3D12Object *pObj, const std::string &name);
}

