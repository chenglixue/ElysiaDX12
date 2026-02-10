#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#define GROUP_SIZE 8
#define IRRADIANCE_GROUP_SIZE 8
#define DISTANCE_GROUP_SIZE 8
#define DXR_Max 10000

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridSpacing;
    float4 g_GridOrigin;
    float4 g_GridDimensions;

    uint g_RayDataBufferIndex;
    float g_RandomRotation;
}

RaytracingAccelerationStructure g_SceneTLAS : register(t0);
StructuredBuffer<InstanceData> g_InstanceDataBuffer : register(t1);
StructuredBuffer<Vector3> g_ProbeOffsetBuffer : register(t2);

SamplerState g_WarpPointSampler : register(s0);
SamplerState g_ClampPointSampler : register(s1);
SamplerState g_WarpLinearSampler : register(s2);
SamplerState g_ClampLinearSampler : register(s3);
SamplerState g_WarpAnisotropicSampler : register(s4);
SamplerState g_ClampAnisotropicSampler : register(s5);
SamplerState g_ShadowWarpLinearSampler : register(s6);
SamplerState g_ShadowClampLinearSampler : register(s7);

// void Elysia_DDGI_StoreIrradiance(uint2 id, float3 val)
// {
//     RWTexture2D<float4> o = ResourceDescriptorHeap[g_IrradianceTexIndex];
//     o[id].rgb = val;
// }

void Elysia_DDGI_StoreRayData(uint writeIndex, float3 radiance, float distance)
{
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    rayDatas[writeIndex].Radiance = radiance;
    rayDatas[writeIndex].Distance = distance;
}
RayData Elysia_DDGI_LoadRayData(uint readIndex)
{
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    return rayDatas[readIndex];
}

[shader("raygeneration")]
void GenerateRayMain()
{
    uint probeIndex = DispatchRaysIndex().x;
    uint rayIndex = DispatchRaysIndex().y;
    UINT2 dimension = DispatchRaysDimensions().xy;

    Vector3 rayOrigin = GetProbeWorldPosition(probeIndex,
                                              g_GridOrigin,
                                              g_GridSpacing,
                                              g_GridDimensions) + g_ProbeOffsetBuffer[probeIndex];
    float3 rayDir = SphericalFibonacci(rayIndex, Rays_Per_Probe, g_RandomRotation);

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = rayDir;
    rayDesc.TMin = 1e-4;
    rayDesc.TMax = DXR_Max;

    RayData rayData;
    rayData.Radiance = 0.f;
    rayData.Distance = 0.f;

    TraceRay(g_SceneTLAS,
             RAY_FLAG_NONE,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             rayData);

    uint writeIndex = probeIndex * Rays_Per_Probe + rayIndex;
    Elysia_DDGI_StoreRayData(writeIndex, rayData.Radiance, rayData.Distance);
}

[shader("miss")]
void RayMiss(inout RayData rayData)
{
    rayData.Radiance = 0.f;
    rayData.Distance = DXR_Max;
}

[shader("closesthit")]
void RayClosestHit(inout RayData rayData,
                   in BuiltInTriangleIntersectionAttributes attr)
{
    UINT instanceID = InstanceID();
    uint primIdx = PrimitiveIndex();

    InstanceData instanceData = g_InstanceDataBuffer[instanceID];

    StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[instanceData.VertexBufferIndex];
    StructuredBuffer<uint> indices = ResourceDescriptorHeap[instanceData.IndexBufferIndex];
    UINT vertexOffset = instanceData.VertexOffset;
    UINT indexOffset = instanceData.IndexOffset;

    UINT i0 = indices[indexOffset + primIdx * 3 + 0];
    UINT i1 = indices[indexOffset + primIdx * 3 + 1];
    UINT i2 = indices[indexOffset + primIdx * 3 + 2];
    Vertex v0 = vertices[vertexOffset + i0];
    Vertex v1 = vertices[vertexOffset + i1];
    Vertex v2 = vertices[vertexOffset + i2];

    float2 uv0 = v0.uv;
    float2 uv1 = v1.uv;
    float2 uv2 = v2.uv;

    // 重心坐标插值
    float2 bary = attr.barycentrics; // 这是 (u, v)
    float w = 1.0 - bary.x - bary.y;
    float2 finalUV = uv0 * w + uv1 * bary.x + uv2 * bary.y;

    float4 baseColorAlpha = SampleTexture2D_LOD(instanceData.BaseColorTexIndex,
                                                finalUV,
                                                g_WarpLinearSampler,
                                                0);

    rayData.Radiance = baseColorAlpha.rgb;
    rayData.Distance = RayTCurrent();

    bool isBackFace = (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE);
    if (isBackFace)
    {
        rayData.Distance *= -1.0f; // 用负数标记背面撞击
        rayData.Radiance = 0.0f;
    }
}