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

    UINT g_RelocationLUTIndex;
    UINT g_ProbeStatesIndex;
    float g_ProbeNormalBias;
    float g_ProbeViewBias;

    float g_DDGIEncodingGamma;
    UINT g_SkyboxTexIndex;
    UINT g_GIDataBufferIndex;
    UINT g_ProbeOffsetsIndex;
}

RaytracingAccelerationStructure g_SceneTLAS : register(t0);
StructuredBuffer<InstanceData> g_InstanceDataBuffer : register(t1);
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
    if (probeIndex % 4 != frameIndex % 4)
        return;

    [branch]
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RELOCATE_RAY_COUNT)
        return;

    Vector3 rayOrigin = GetProbeWorldPosition(probeIndex,
                                              g_GridOrigin,
                                              g_GridSpacing,
                                              g_GridDimensions);

    RayDesc rayDesc;
    rayDesc.Origin = rayOrigin;
    rayDesc.Direction = DDGIGetProbeRayDir(rayIndex, RAYS_PER_PROBE, g_RandomRotation);
    rayDesc.TMin = 0.f;
    rayDesc.TMax = DXR_MAX;

    RayData packRayData = (RayData)0;
    RAY_FLAG rayFlag = RAY_FLAG_NONE;
    TraceRay(g_SceneTLAS,
             rayFlag,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             packRayData);

    RWStructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];

    if (packRayData.hitDist < 0.f)
    {
        TextureCube<float4> skyboxTex = ResourceDescriptorHeap[g_SkyboxTexIndex];
        float3 skyRadiance = skyboxTex.SampleLevel(g_WarpLinearSampler, WorldRayDirection(), 0).rgb;
        DDGI_Store_Probe_RAY_MISS(GIDataBuffer, writeIndex, 0);
        return;
    }

    if (packRayData.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        DDGI_Store_Probe_RAY_BackFace_Hit(GIDataBuffer, writeIndex, packRayData.hitDist);
        return;
    }

    if (rayIndex < RELOCATE_RAY_COUNT)
    {
        DDGI_Store_Probe_RAY_FrontFace_Hit(GIDataBuffer, writeIndex, packRayData.hitDist);
        return;
    }

    // direct light
    float3 directIrradiance = directDiffuseLight(packRayData,
                                                 g_ProbeNormalBias,
                                                 g_ProbeViewBias,
                                                 g_SceneTLAS,
                                                 GetMainLight(mainLight));

    // indirect light
    float3 indirectIrradiance = 0.f;
    float blendWeight = DDGIGetVolumeBlendWeight(packRayData.Position,
                                                 g_GridOrigin,
                                                 g_GridSpacing,
                                                 0,
                                                 float4(0, 0, 0, 1));
    if (blendWeight > 0.f)
    {
        float3 surfaceBias = DDGIGetSurfaceBias(packRayData.Normal,
                                                rayDesc.Direction,
                                                g_ProbeNormalBias,
                                                g_ProbeViewBias);
        indirectIrradiance += SampleDDGI(
            packRayData.Position,
            packRayData.Normal,
            surfaceBias,
            g_GridOrigin,
            g_GridSpacing,
            g_GridDimensions,
            g_DDGIEncodingGamma,
            g_IrradianceTexSize,
            g_IrradianceTexIndex,
            g_DistanceTexSize,
            g_DistanceTexIndex,
            g_ProbeOffsetsIndex,
            g_ProbeStatesIndex,
            WarpLinearSampler
            );
        indirectIrradiance *= blendWeight;
    }

    float maxAlbedo = 0.9f;
    indirectIrradiance = min(packRayData.Albedo.rgb, maxAlbedo) / PI * indirectIrradiance;

    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    rayDatas[writeIndex] = packRayData;

    DDGI_Store_Probe_RAY_FrontFace_Hit(GIDataBuffer,
                                       writeIndex,
                                       packRayData.hitDist,
                                       directIrradiance + indirectIrradiance);
}

[shader("miss")]
void ShadowMiss(inout ShadowRayload shadowRayload)
{
    shadowRayload.isHit = false;
}
[shader("miss")]
void RayMiss(inout RayData rayData)
{
    rayData.hitDist = -1.f;
}

[shader("closesthit")]
void RayClosestHit(inout RayData rayData,
                   in BuiltInTriangleIntersectionAttributes attr)
{
    rayData.hitDist = RayTCurrent();
    rayData.hitKind = HitKind();

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

    float3 positionWS = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    rayData.Position = positionWS;

    float3 normalOS = v.normalOS;
    float3 N = normalize(mul(ObjectToWorld3x4(), float4(normalOS, 0.f)));
    rayData.Normal.xyz = N;

    if (instanceData.BaseColorTexIndex > 0)
    {
        uint width, height, numLevels;
        Texture2D<float4> albedoTex = ResourceDescriptorHeap[instanceData.BaseColorTexIndex];
        albedoTex.GetDimensions(0, width, height, numLevels);

        float4 albedoOpacity = albedoTex.SampleLevel(g_WarpLinearSampler, v.uv, numLevels / 2.f);
        rayData.Albedo = albedoOpacity.xyz * albedoOpacity.w;
    }

    if (instanceData.NormalTexIndex > 0)
    {
        uint width, height, numLevels;
        Texture2D<float4> normalTex = ResourceDescriptorHeap[instanceData.NormalTexIndex];
        normalTex.GetDimensions(0, width, height, numLevels);

        float3 tangent = normalize(mul(ObjectToWorld3x4(), float4(v.tangentOS.xyz, 0.f)));
        float3 bitTangent = cross(N, tangent) * v.tangentOS.w;
        float3x3 TBN = {tangent, bitTangent, N};

        float3 normalTS = normalTex.SampleLevel(g_WarpLinearSampler, v.uv, numLevels / 2.f);
        normalTS = normalTS * 2.f - 1.f;
        float3 shadingNormal = mul(normalTS, TBN);
        rayData.ShadingNormal.xyz = shadingNormal;
    }
}