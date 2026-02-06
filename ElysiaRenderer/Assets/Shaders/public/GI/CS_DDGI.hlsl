#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#define GROUP_SIZE 8
#define IRRADIANCE_GROUP_SIZE 8
#define DISTANCE_GROUP_SIZE 8
#define PROBE_COUNT 1024
#define Rays_Per_Probe 32

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector3 g_GridSpacing;
    Vector3 g_GridOrigin;
    Vector3 g_GridDimensions;

    UINT g_TargetTexIndex;
    UINT g_IrradianceTexIndex;
}

void Elysia_DDGI_StoreIrradiance(UINT2 id, float3 val)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_IrradianceTexIndex];
    o[id].rgb = val;
}

[numthreads(IRRADIANCE_GROUP_SIZE, IRRADIANCE_GROUP_SIZE, 1)]
void GenerateRay(uint3 groupID : SV_GroupID,
                 uint3 groupThreadID : SV_GroupThreadID,
                 UINT3 id : SV_DispatchThreadID)
{
    uint probeIndex = groupID.x;
    // 2. 获取当前线程处理的像素在探针内部的坐标 (0~7, 0~7)
    uint2 texelPosInProbe = groupThreadID.xy;

    float3 debugColor = float3(
        (probeIndex % 16) / 15.0f,
        (probeIndex / 16) / 63.0f,
        0.0f
        );

    uint probesPerRow = g_GridDimensions.x;
    uint2 probeGridCoord = uint2(probeIndex % probesPerRow, probeIndex / probesPerRow);
    uint2 globalTexelPos = probeGridCoord * 8 + texelPosInProbe;

    Elysia_DDGI_StoreIrradiance(globalTexelPos, debugColor);
}