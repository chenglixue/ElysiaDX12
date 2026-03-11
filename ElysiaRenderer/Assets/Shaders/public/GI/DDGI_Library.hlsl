#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include "public\GI\Irradiance.hlsl"

#define GROUP_SIZE 8
#define IRRADIANCE_GROUP_SIZE 8
#define DISTANCE_GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridSpacing;
    float4 g_GridOrigin;
    float4 g_GridDimensions;
    float4 g_IrradianceTexSize;
    float4 g_DistanceTexSize;
    Vector4 g_RandomRotation;

    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetIndexTexIndex;

    float g_ProbeNormalBias;
    float g_ProbeViewBias;
    float g_DDGIEncodingGamma;
}

RaytracingAccelerationStructure g_SceneTLAS : register(t0);
StructuredBuffer<InstanceData> g_InstanceDataBuffer : register(t1);
// StructuredBuffer<Vector3> g_ProbeOffsetBuffer : register(t2);
StructuredBuffer<UINT> g_ProbeStatesBuffer : register(t2);
StructuredBuffer<Vector4> g_ProbeRelocationLUTBuffer : register(t3);

SamplerState g_WarpPointSampler : register(s0);
SamplerState g_ClampPointSampler : register(s1);
SamplerState g_WarpLinearSampler : register(s2);
SamplerState g_ClampLinearSampler : register(s3);
SamplerState g_WarpAnisotropicSampler : register(s4);
SamplerState g_ClampAnisotropicSampler : register(s5);
SamplerState g_ShadowWarpLinearSampler : register(s6);
SamplerState g_ShadowClampLinearSampler : register(s7);

UINT Elysia_DDGI_LoadeProbeState(UINT probeIndex)
{
    return g_ProbeStatesBuffer[probeIndex];
}

[shader("raygeneration")]
void GenerateRayMain()
{
    uint probeIndex = DispatchRaysIndex().x;

    uint rayIndex = DispatchRaysIndex().y;
    // UINT2 dimension = DispatchRaysDimensions().xy;
    // uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);
    UINT probeState = Elysia_DDGI_LoadeProbeState(probeIndex);

    [branch]
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RELOCATE_RAY_COUNT)
        return;

    UINT2 probeOffsetIndexID = UINT2(probeIndex % 64, probeIndex * rcp(64));
    RWTexture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
    float3 probeOffset = g_ProbeRelocationLUTBuffer[index];

    Vector3 rayOrigin = GetProbeWorldPosition(probeIndex,
                                              g_GridOrigin,
                                              g_GridSpacing,
                                              g_GridDimensions) + probeOffset;
    float distToCamera = distance(rayOrigin, cameraPosWS);
    uint updateInterval = 64;

    if (distToCamera < NEAR_GI_DISTANCE)
    {
        updateInterval = 4;
    }
    else if (distToCamera < MIDDLE_GI_DISTANCE)
    {
        updateInterval = 16;
    }
    [branch]
    if (probeIndex % updateInterval != frameIndex % updateInterval)
        return;

    float3 rayDir = DDGIGetProbeRayDir(rayIndex, RAYS_PER_PROBE, g_RandomRotation);

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = rayDir;
    rayDesc.TMin = 0.f;
    rayDesc.TMax = DXR_MAX;

    RayData rayData = (RayData)0;

    RAY_FLAG rayFlag = RAY_FLAG_NONE | RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;
    TraceRay(g_SceneTLAS,
             RAY_FLAG_NONE,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             rayData);

    uint writeIndex = probeIndex * RAYS_PER_PROBE + rayIndex;
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    rayDatas[writeIndex].Data = rayData.Data;
    rayDatas[writeIndex].Position = rayData.Position;
    // Elysia_DDGI_StoreRayData(writeIndex, rayData.Radiance, rayData.Distance);
}

[shader("miss")]
void RayMiss(inout RayData rayData)
{
    rayData.Position = float4(0.f, 0.f, 0.f, DXR_MAX);
    rayData.Data = float4(0.f, 0.f, 0.f, 0.f);
}

[shader("miss")]
void ShadowMiss(inout ShadowRayload shadowRayload)
{
    shadowRayload.isHit = false;
}

[shader("closesthit")]
void RayClosestHit(inout RayData rayData,
                   in BuiltInTriangleIntersectionAttributes attr)
{
    [branch]
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        rayData.Position = float4(0.f, 0.f, 0.f, -RayTCurrent() * 0.2f); // 负数标记背面撞击
        rayData.Data = float4(0.f, 0.f, 0.f, 0.f);

        return;
    }

    UINT instanceID = InstanceID();
    uint primIdx = PrimitiveIndex();
    uint globalGeometryIdx = instanceID + primIdx;
    InstanceData instanceData = g_InstanceDataBuffer[globalGeometryIdx];
    StructuredBuffer<Vertex> verticesBuffer = ResourceDescriptorHeap[instanceData.VertexBufferIndex];
    StructuredBuffer<uint> indicesBuffer = ResourceDescriptorHeap[instanceData.IndexBufferIndex];
    UINT vertexOffset = instanceData.VertexOffset;
    UINT indexOffset = instanceData.IndexOffset;

    // UINT i0 = indicesBuffer[indexOffset + primIdx * 3 + 0];
    // UINT i1 = indicesBuffer[indexOffset + primIdx * 3 + 1];
    // UINT i2 = indicesBuffer[indexOffset + primIdx * 3 + 2];
    // Vertex vertices[3];
    // vertices[0] = verticesBuffer[vertexOffset + i0];
    // vertices[1] = verticesBuffer[vertexOffset + i1];
    // vertices[2] = verticesBuffer[vertexOffset + i2];
    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    Vertex v = InterpolateVertex(verticesBuffer[vertexOffset + indicesBuffer[indexOffset + PrimitiveIndex() * 3 + 0]],
                                 verticesBuffer[vertexOffset + indicesBuffer[indexOffset + PrimitiveIndex() * 3 + 1]],
                                 verticesBuffer[vertexOffset + indicesBuffer[indexOffset + PrimitiveIndex() * 3 + 2]],
                                 bary);

    float3 normalOS = v.normalOS;
    float3 N = normalize(mul(ObjectToWorld3x4(), float4(normalOS, 0.f)));

    float3 positionWS = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    rayData.Position = float4(positionWS, RayTCurrent());
    rayData.Data = float4(PackNormal(N),
                          instanceData.BaseColorTexIndex,
                          instanceData.NormalTexIndex,
                          0.f);
}