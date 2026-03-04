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
UINT Elysia_DDGI_LoadeProbeState(UINT probeIndex)
{
    return g_ProbeStatesBuffer[probeIndex];
}

[shader("raygeneration")]
void GenerateRayMain()
{
    uint probeIndex = DispatchRaysIndex().x;
    [branch]
    if (probeIndex % 4 != frameIndex % 4)
        return;

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
                                              g_GridDimensions + probeOffset);
    // g_ProbeOffsetBuffer[probeIndex];
    float3 rayDir = DDGIGetProbeRayDir(rayIndex, RAYS_PER_PROBE, g_RandomRotation);

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = rayDir;
    rayDesc.TMin = 0.f;
    rayDesc.TMax = DXR_MAX;

    RayData rayData;
    rayData.Radiance = 0.f;
    rayData.Distance = 0.f;

    UINT rayFlag = RAY_FLAG_NONE;
    TraceRay(g_SceneTLAS,
             rayFlag,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             rayData);

    uint writeIndex = probeIndex * RAYS_PER_PROBE + rayIndex;
    Elysia_DDGI_StoreRayData(writeIndex, rayData.Radiance, rayData.Distance);
}

[shader("miss")]
void RayMiss(inout RayData rayData)
{
    rayData.Radiance = 0.f;
    rayData.Distance = DXR_MAX;
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
        rayData.Distance = -rayData.Distance * 0.2f; // 用负数标记背面撞击
        rayData.Radiance = 0.0f;
        return;
    }

    UINT instanceID = InstanceID();
    uint primIdx = PrimitiveIndex();
    uint globalGeometryIdx = instanceID + GeometryIndex();
    InstanceData instanceData = g_InstanceDataBuffer[globalGeometryIdx];
    StructuredBuffer<Vertex> verticesBuffer = ResourceDescriptorHeap[instanceData.VertexBufferIndex];
    StructuredBuffer<uint> indicesBuffer = ResourceDescriptorHeap[instanceData.IndexBufferIndex];
    UINT vertexOffset = instanceData.VertexOffset;
    UINT indexOffset = instanceData.IndexOffset;

    UINT i0 = indicesBuffer[indexOffset + primIdx * 3 + 0];
    UINT i1 = indicesBuffer[indexOffset + primIdx * 3 + 1];
    UINT i2 = indicesBuffer[indexOffset + primIdx * 3 + 2];

    Vertex vertices[3];
    vertices[0] = verticesBuffer[vertexOffset + i0];
    vertices[1] = verticesBuffer[vertexOffset + i1];
    vertices[2] = verticesBuffer[vertexOffset + i2];
    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    Vertex v = InterpolateVertex(vertices, bary);

    // 重心坐标插值
    float2 sampleUV = v.uv;
    // float3 normalOS = v.normalOS;
    // float3 tangentOS = v.tangentOS;

    float4 baseColorAlpha = SampleTexture2D_LOD(instanceData.BaseColorTexIndex,
                                                sampleUV,
                                                WarpLinearSampler,
                                                0);
    [branch]
    if (baseColorAlpha.a < 0.1f)
    {
        rayData.Radiance = 0;
        rayData.Distance = RayTCurrent();
        return;
    }

    // float3 N = normalize(mul(ObjectToWorld3x4(), float4(normalOS, 0.f)));
    // if (instanceData.NormalTexIndex > 0)
    // {
    //     float3 T = normalize(mul(ObjectToWorld3x4(), float4(tangentOS, 0.f)));
    //     float3 B = cross(N, T) * v.tangentOS.w;
    //
    //     float3x3 TBN = {T, B, N};
    //     N = SampleTexture2D_LOD(instanceData.NormalTexIndex, sampleUV, g_WarpLinearSampler, 0);
    //     N = N * 2.f - 1.f;
    //     N = mul(N, TBN);
    // }

    // LightData mainLightData = GetMainLight(mainLight);
    // float NoL = max(0, dot(N, mainLightData.toLight));
    //
    // float3 positionWS = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    // float shadow = DDGI_Shadow_Visibity(positionWS,
    //                                     N,
    //                                     g_ProbeNormalBias,
    //                                     mainLightData.toLight,
    //                                     g_SceneTLAS);
    // float3 directRadiance = baseColorAlpha.rgb / PI * mainLightData.color * mainLightData.intensity * NoL * shadow;
    //
    // float blendWeight = DDGIGetVolumeBlendWeight(positionWS, g_GridOrigin, g_GridSpacing, 0, float4(0, 0, 0, 1));
    // if (blendWeight > 0.f)
    // {
    //     float3 indirectIrradiance = SampleDDGI(
    //         positionWS,
    //         N,
    //         DDGIGetSurfaceBias(N,
    //                            -WorldRayDirection(),
    //                            g_ProbeNormalBias,
    //                            g_ProbeViewBias),
    //         g_GridOrigin,
    //         g_GridSpacing,
    //         g_GridDimensions,
    //         g_DDGIEncodingGamma,
    //         g_IrradianceTexSize,
    //         g_IrradianceTexIndex,
    //         g_DistanceTexSize,
    //         g_DistanceTexIndex,
    //         g_ProbeOffsetIndexTexIndex,
    //         g_ProbeRelocationLUTBuffer,
    //         g_ProbeStatesBuffer,
    //         g_WarpLinearSampler
    //         );
    //     float maxAlbedo = 0.9f;
    //     float3 indirectRadiance = min(baseColorAlpha.rgb, maxAlbedo) / PI * indirectIrradiance;
    //
    //     indirectRadiance *= blendWeight;
    //     rayData.Radiance += indirectRadiance;
    // }
    rayData.Radiance = baseColorAlpha;
    rayData.Distance = RayTCurrent();
}