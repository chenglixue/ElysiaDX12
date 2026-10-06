#ifndef CASCOMMON_H
#define CASCOMMON_H

#include "private\ShadingCommon.hlsl"

void Elyisa_CAS_Save(UINT texIndex, int2 writePos, float3 saveColor)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[texIndex];
    o[writePos] = float4(saveColor, 1.f);
}

#endif