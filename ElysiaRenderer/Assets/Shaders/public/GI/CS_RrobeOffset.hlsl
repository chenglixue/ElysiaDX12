#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_ProbeOffsetIndexTexIndex;
}

void DDGI_Store_Probe_Offset_Index(UINT2 id, uint value)
{
    RWTexture2D<UINT> probeOffsetTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    probeOffsetTex[id] = value;
}

UINT DDGI_Load_Probe_Offset_Index(UINT2 id)
{
    RWTexture2D<UINT> probeOffsetTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    return probeOffsetTex[id];
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ResetProbeOffsetIndex(UINT3 id : SV_DispatchThreadID)
{
    DDGI_Store_Probe_Offset_Index(id, 0);
}