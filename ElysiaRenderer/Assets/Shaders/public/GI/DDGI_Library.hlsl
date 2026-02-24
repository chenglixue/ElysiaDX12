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

    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    float g_RandomRotation;

    float g_ProbeNormalBias;
    float g_ProbeViewBias;
    float g_DDGIEncodingGamma;
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
    uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);

    Vector3 rayOrigin = GetProbeWorldPosition(probeIndex,
                                              g_GridOrigin,
                                              g_GridSpacing,
                                              g_GridDimensions) + g_ProbeOffsetBuffer[probeIndex];
    float3 rayDir = DDGIGetProbeRayDir(rayIndex, RAYS_PER_PROBE, gridIdx, frameIndex, false);

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = rayDir;
    rayDesc.TMin = 0.01f;
    rayDesc.TMax = DXR_MAX;

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
    UINT instanceID = InstanceID();
    uint primIdx = PrimitiveIndex();

    uint globalGeometryIdx = instanceID + GeometryIndex();
    InstanceData instanceData = g_InstanceDataBuffer[globalGeometryIdx];

    bool isBackFace = (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE);
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
    float3 normalOS = v0.normalOS * w + v1.normalOS * bary.x + v2.normalOS * bary.y;
    float3 N = normalize(mul((float3x3)ObjectToWorld3x4(), normalOS));

    if (isBackFace)
    {
        N = -N;
    }

    LightData mainLightData = GetMainLight(mainLight);
    float3 toLight = mainLightData.toLight;
    float NoL = dot(N, toLight);

    float3 directRadiance = 0.f;
    float3 positionWS = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    if (NoL > 0.f)
    {
        float shadow = DDGI_Shadow_Visibity(positionWS, N, toLight, g_SceneTLAS);
        directRadiance = baseColorAlpha.rgb * mainLightData.color * mainLightData.intensity * NoL *
                         shadow;
    }

    float blendWeight = DDGIGetVolumeBlendWeight(positionWS, g_GridOrigin, g_GridSpacing, 0, float4(0, 0, 0, 1));

    if (blendWeight > 0.f)
    {
        float3 indirectIrradiance = SampleDDGI(
            positionWS,
            N,
            DDGIGetSurfaceBias(N,
                               WorldRayDirection(),
                               g_ProbeNormalBias,
                               g_ProbeViewBias),
            g_GridOrigin,
            g_GridSpacing,
            g_GridDimensions,
            g_DDGIEncodingGamma,
            g_IrradianceTexSize,
            g_IrradianceTexIndex,
            g_DistanceTexSize,
            g_DistanceTexIndex,
            g_ProbeOffsetBuffer,
            g_WarpLinearSampler
            );
        float maxAlbedo = 0.9f;
        float3 indirectRadiance = min(baseColorAlpha.rgb, maxAlbedo) / PI * indirectIrradiance;

        blendWeight = Pow2(blendWeight);
        indirectRadiance *= blendWeight;
        rayData.Radiance += indirectRadiance;
    }
    rayData.Radiance += directRadiance;
    rayData.Distance = RayTCurrent();

    if (isBackFace)
    {
        rayData.Distance *= -1.0f; // 用负数标记背面撞击
        rayData.Radiance = 0.0f;
    }
}