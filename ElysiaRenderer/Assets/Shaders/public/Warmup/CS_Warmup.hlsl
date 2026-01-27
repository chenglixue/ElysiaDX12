#include "private\ShadingCommon.hlsl"

#define AO_GROUP_SIZE 1

cbuffer PassConstant : register(b0, perMaterialSpace)
{
    UINT g_TargetTexIndex;
}

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void WarmUpCompute(uint3 id: SV_DispatchThreadID)
{
    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float val = sin(float(id.x) * 0.1);
    o[id.xy] = val * 0.0001f;
}