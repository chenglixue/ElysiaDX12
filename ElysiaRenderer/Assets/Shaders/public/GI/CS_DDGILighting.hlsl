#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridOrigin;
    float4 g_GridSpacing;
    float4 g_GridDimensions;
    float4 g_RandomRotation;

    float4 g_FullScreenSize;
    float4 g_HalfScreenSize;

    UINT g_ProbeStatesIndex;
    UINT g_StaticAABBIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_RelocationLUTIndex;
    float g_DDGIBlendWeight;
    float g_ProbeIrradianceThreshold;
    float g_ProbeBrightnessThreshold;
    float g_DDGIEncodingGamma;
    UINT g_StaticAABBCount;
    UINT g_DDGI_Probe_Num_Texels;
    bool g_IsBlendIrradiance;
}

[numthreads(8, 8, 1)]
void DDGI_Shading(uint3 id : SV_DispatchThreadID,
                  uint3 GroupThreadID : SV_GroupThreadID,
                  uint3 GroupID : SV_GroupID)
{
    float2 uv = (id.xy + 0.5f) * g_FullScreenSize.zw;

    float depth = SampleTexture2D(OpaqueDepthIndex, ClampPointSampler, )
}