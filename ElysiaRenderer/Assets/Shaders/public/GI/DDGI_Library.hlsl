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
    float4 g_RandomRotation;

    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetIndexTexIndex;

    UINT g_PreReservoirBufferIndex;
    UINT g_CurrReservoirBufferIndex;
    float g_ProbeNormalBias;
    float g_ProbeViewBias;

    float g_DDGIEncodingGamma;
}

RaytracingAccelerationStructure g_SceneTLAS : register(t0);
StructuredBuffer<InstanceData> g_InstanceDataBuffer : register(t1);
// StructuredBuffer<Vector3> g_ProbeOffsetBuffer : register(t2);
StructuredBuffer<UINT> g_ProbeStatesBuffer : register(t2);
StructuredBuffer<Vector4> g_ProbeRelocationLUTBuffer : register(t3);
StructuredBuffer<GIData> g_GIDataBuffer : register(t4);

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

float3 SampleIrradianceTex(float3 positionWS, float3 rayDir)
{
    float3 gridCoord = GetGridCoord(positionWS, g_GridOrigin, g_GridSpacing);
    int3 baseProbeCoords = floor(gridCoord);
    int3 adjCoords = clamp(baseProbeCoords, 0, g_GridDimensions.xyz - 1);
    uint2 atlasPos = uint2(adjCoords.x, adjCoords.y + adjCoords.z * g_GridDimensions.y);

    float2 uv = OctEncode(rayDir);
    uv = (uv * 0.5f + 0.5f) * (DDGI_PROBE_IRRADIANCE_NUM_TEXELS - 2.f) + 1.0f;
    uv = (float2(atlasPos * DDGI_PROBE_IRRADIANCE_NUM_TEXELS) + uv) * g_IrradianceTexSize.zw;
    float3 irradiance = SampleTexture2D_LOD(g_IrradianceTexIndex, uv, ClampLinearSampler, 0);

    return irradiance;
}

[shader("raygeneration")]
void GenerateRayMain()
{
    uint probeIndex = DispatchRaysIndex().x;
    uint rayIndex = DispatchRaysIndex().y;
    uint writeIndex = probeIndex * RAYS_PER_PROBE + rayIndex;
    // UINT2 dimension = DispatchRaysDimensions().xy;
    // uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);
    UINT probeState = Elysia_DDGI_LoadeProbeState(probeIndex);

    [branch]
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RELOCATE_RAY_COUNT)
        return;

    RWTexture2D<uint> ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    UINT2 probeOffsetIndexID = UINT2(probeIndex % 64, probeIndex * rcp(64));
    UINT index = ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
    float3 probeOffset = g_ProbeRelocationLUTBuffer[index];

    Vector3 rayOrigin = GetProbeWorldPosition(probeIndex,
                                              g_GridOrigin,
                                              g_GridSpacing,
                                              g_GridDimensions) + probeOffset;
    float distToCamera = distance(rayOrigin, cameraPosWS);

    uint updateInterval = 16;
    if (distToCamera < NEAR_GI_DISTANCE)
    {
        updateInterval = 4;
    }
    else if (distToCamera < MIDDLE_GI_DISTANCE)
    {
        updateInterval = 8;
    }
    // [branch]
    // if (probeIndex % updateInterval != frameIndex % updateInterval)
    //     return;

    Texture2D<float4> blueNoiseTex = ResourceDescriptorHeap[BlueNoiseTexIndex];
    float w, h;
    blueNoiseTex.GetDimensions(w, h);
    uint hash = probeIndex ^ (frameIndex * 19349663);
    float2 offset = float2(hash & 0xFFFF, (hash >> 16) & 0xFFFF);
    float2 blueNoiseUV = (offset + float2(rayIndex, rayIndex * 7.1f)) / float2(w, h);
    float3 blueNoise = blueNoiseTex.SampleLevel(g_WarpPointSampler, blueNoiseUV, 0);

    float3 finalRayDir = 0.f;
    float pdf = 0.f;
    float finalWeight = 0.f;
    {
        const uint N = 4;
        float3 candidateDirs[N];
        float weights[N];
        float sumWeight = 0.0f;

        for (UINT i = 0; i < N; ++i)
        {
            candidateDirs[i] = DDGIGetProbeRayDir(rayIndex + i * N, RAYS_PER_PROBE, g_RandomRotation);

            weights[i] = Luminance(SampleIrradianceTex(rayOrigin, candidateDirs[i])) + 0.1f;
            weights[i] *= Luminance(g_GIDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex + i * N].Irradiance) + 0.1f;
            sumWeight += weights[i];
        }

        float randomWeight = blueNoise.r * sumWeight;
        float cumulativeWeight = 0.f;
        finalRayDir = candidateDirs[0];
        finalWeight = weights[0];

        for (UINT i = 0; i < N; ++i)
        {
            cumulativeWeight += weights[i];
            if (randomWeight <= cumulativeWeight)
            {
                finalRayDir = candidateDirs[i];
                finalWeight = weights[i];
                break;
            }
        }

        pdf = (finalWeight / (sumWeight + 1e-4)) * N * INV_FOUR_PI;
    }

    RWStructuredBuffer<Reservoir> preReservoirBuffer = ResourceDescriptorHeap[g_PreReservoirBufferIndex];
    RWStructuredBuffer<Reservoir> currReservoirBuffer = ResourceDescriptorHeap[g_CurrReservoirBufferIndex];
    Reservoir currReservoir = (Reservoir)0;
    currReservoir.weightSum = finalWeight / (pdf + 1e-4);
    currReservoir.numSamples = 1.f;
    Reservoir preReservoir = preReservoirBuffer[writeIndex];

    float sampleLimit = 30.f;
    preReservoir.numSamples = min(preReservoir.numSamples, sampleLimit);

    float preWeight = Luminance(g_GIDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex].Irradiance) + 0.1f;
    float preAccuWeight = preWeight * preReservoir.estimatorWeight * preReservoir.numSamples;

    currReservoir.weightSum += preAccuWeight;
    currReservoir.numSamples += preReservoir.numSamples;

    float random = blueNoise.g;
    if (random < preAccuWeight / currReservoir.weightSum)
    {
        finalRayDir = preReservoir.direction;
        finalWeight = preWeight;
    }

    int3 gridCoord = int3(floor((rayOrigin - g_GridOrigin) / g_GridSpacing));
    int3 neighborOffset = int3(blueNoise.b * 3.0f - 1.0f,
                               frac(blueNoise.b * 13.1f) * 3.0f - 1.0f,
                               frac(blueNoise.b * 47.7f) * 3.0f - 1.0f);
    int3 neighborCoord = clamp(gridCoord + neighborOffset, 0, (int3)g_GridDimensions - 1);
    uint neighborProbeIdx = neighborCoord.x + neighborCoord.y * g_GridDimensions.x + neighborCoord.z * g_GridDimensions.
                            x * g_GridDimensions.y;

    if (neighborProbeIdx != probeIndex)
    {
        UINT neighborWriteIdx = neighborProbeIdx * RAYS_PER_PROBE + rayIndex;
        Reservoir neighborReservoir = preReservoirBuffer[neighborWriteIdx];
        neighborReservoir.numSamples = min(neighborReservoir.numSamples, sampleLimit);

        UINT2 neighborProbeOffsetIndexID = UINT2(neighborProbeIdx % 64, neighborProbeIdx * rcp(64));
        UINT neighborOffsetIndex = ProbeOffsetIndexTex.Load(UINT3(neighborProbeOffsetIndexID, 0));
        float3 neighborProbeOffset = g_ProbeRelocationLUTBuffer[neighborOffsetIndex];

        float3 neighborPos = GetProbeWorldPosition(neighborProbeIdx, g_GridOrigin, g_GridSpacing, g_GridDimensions) +
                             neighborProbeOffset;
        float3 hitPos = neighborPos + neighborReservoir.direction * neighborReservoir.hitDistance;

        float3 dirToHit = hitPos - rayOrigin;
        float distToHitSq = dot(dirToHit, dirToHit);
        float distToHit = sqrt(distToHitSq);
        float3 dirToHitNor = dirToHit / distToHit;

        RayDesc shadowRay = (RayDesc)0;
        shadowRay.Origin = rayOrigin;
        shadowRay.Direction = dirToHitNor;
        shadowRay.TMin = 0.f;
        shadowRay.TMax = distToHit;

        ShadowRayload shadowPayload;
        shadowPayload.isHit = true;

        TraceRay(g_SceneTLAS,
                 RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                 0xFF,
                 1,
                 0,
                 1,
                 shadowRay,
                 shadowPayload);

        [branch]
        if (!shadowPayload.isHit)
        {
            float3 hitNomral = UnpackNormal(neighborReservoir.packedNormal);
            float cosNeighbor = saturate(dot(-neighborReservoir.direction, hitNomral));
            float cosCurr = saturate(dot(-dirToHitNor, hitNomral));
            // 雅可比修正
            float jacobian = (neighborReservoir.hitDistance * neighborReservoir.hitDistance) / max(1e-6f, distToHitSq);
            jacobian *= cosCurr / cosNeighbor;

            float neighborWeight = Luminance(g_GIDataBuffer[neighborWriteIdx].Irradiance) + 0.1f;
            float neighborAccuWeight = neighborWeight * neighborReservoir.numSamples
                                       * neighborReservoir.estimatorWeight * jacobian;
            currReservoir.weightSum += neighborAccuWeight;
            currReservoir.numSamples += neighborReservoir.numSamples;

            if (blueNoise.b < neighborAccuWeight / currReservoir.weightSum)
            {
                finalRayDir = dirToHitNor;
                finalWeight = neighborWeight;
            }
        }
    }

    currReservoir.direction = finalRayDir;
    currReservoir.estimatorWeight = currReservoir.weightSum / (currReservoir.numSamples * finalWeight + 1e-6f);
    currReservoirBuffer[writeIndex] = currReservoir;

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = currReservoir.direction;
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

    currReservoirBuffer[writeIndex].hitDistance = rayData.Position.w;
    if (rayData.Position.w < 0.f)
    {
        currReservoirBuffer[writeIndex].weightSum = 0.f;
        currReservoirBuffer[writeIndex].estimatorWeight = 0.f;
        currReservoirBuffer[writeIndex].hitDistance = DXR_MAX;
        currReservoirBuffer[writeIndex].packedNormal = rayData.Data.r;
    }
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    rayDatas[writeIndex].Data = rayData.Data;
    rayDatas[writeIndex].Data.w = currReservoir.estimatorWeight;
    rayDatas[writeIndex].Position = rayData.Position;
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

    float hitDistance = RayTCurrent();
    float3 positionWS = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    UINT instanceID = InstanceID();
    uint primIdx = PrimitiveIndex();
    uint globalGeometryIdx = instanceID + primIdx;
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

    float3 normalOS = v.normalOS;
    float3 N = normalize(mul(ObjectToWorld3x4(), float4(normalOS, 0.f)));

    rayData.Position = float4(positionWS, hitDistance);
    rayData.Data = float4(PackNormal(N),
                          instanceData.BaseColorTexIndex,
                          instanceData.NormalTexIndex,
                          0.f);
}