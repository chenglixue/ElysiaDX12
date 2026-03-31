#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"
#include <private\Light.hlsl>
#include "public\GI\Irradiance.hlsl"

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_GridOrigin;
    float4 g_GridSpacing;
    float4 g_GridDimensions;

    float4 g_IrradianceTexSize;
    float4 g_DistanceTexSize;

    UINT g_ProbeStatesIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_RelocationLUTIndex;
    UINT g_GIDataBufferIndex;
    UINT g_CompactedRayBufferIndex;
    UINT g_CompactedIndicesBufferIndex;
    UINT g_GlobalCounterBufferIndex;

    float g_ProbeNormalBias;
    float g_ProbeViewBias;
    float g_DDGIEncodingGamma;
}

RaytracingAccelerationStructure g_SceneTLAS : register(t0, perPassSpace);

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

[numthreads(64, 1, 1)]
void DDGI_Shading(uint3 id : SV_DispatchThreadID)
{

}