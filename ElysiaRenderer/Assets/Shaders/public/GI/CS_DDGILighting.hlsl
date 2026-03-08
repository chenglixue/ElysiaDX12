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
void DDGI_Shading(uint3 id : SV_DispatchThreadID,
                  uint3 GroupThreadID : SV_GroupThreadID,
                  uint3 GroupID : SV_GroupID)
{
    UINT probeIndex = id.x / RAYS_PER_PROBE;
    UINT rayIndex = id.x % RAYS_PER_PROBE;
    [branch]
    if (id.x >= PROBE_COUNT * RAYS_PER_PROBE)
        return;

    RWStructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
    StructuredBuffer<RayData> RayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];
    RayData rayData = RayDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex];
    float distance = rayData.Position.w;
    GIDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex].Distance = distance;
    [branch]
    if (distance < 0.f || distance >= DXR_MAX)
        return;

    [branch]
    if (probeIndex % 4 != frameIndex % 4)
        return;

    StructuredBuffer<UINT> ProbeStatesBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
    uint probeState = ProbeStatesBuffer[probeIndex];
    [branch]
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RELOCATE_RAY_COUNT)
        return;

    UINT baseColorTexIndex = rayData.Data.g;
    UINT normalTexIndex = rayData.Data.b;
    float3 normalWS = UnpackNormal(rayData.Position.r);
    float3 positionWS = rayData.Position.xyz;
    float4 positionVS = mul(float4(positionWS, 1.f), viewMatrix);
    float4 positionCS = mul(positionVS, projMatrix);
    float2 positionNDC = positionCS * rcp(positionCS.w);
    float2 uv = positionNDC * float2(1.f, -1.f) * 0.5f + 0.5f;
    float3 baseColorAlpha = SampleTexture2D(baseColorTexIndex, uv, WarpLinearSampler);

    Vector3 viewDirWS = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    LightData mainLightData = GetMainLight(mainLight);
    float NoL = max(0, dot(normalWS, mainLightData.toLight));
    float shadow = DDGI_Query_Shadow_Visibity(positionWS,
                                              normalWS,
                                              g_ProbeNormalBias,
                                              mainLightData.toLight,
                                              g_SceneTLAS);
    float3 directIrradiance = baseColorAlpha.rgb / PI * mainLightData.color * mainLightData.intensity * NoL * shadow;
    float3 result = directIrradiance;

    float blendWeight = DDGIGetVolumeBlendWeight(positionWS, g_GridOrigin, g_GridSpacing, 0, float4(0, 0, 0, 1));
    if (blendWeight > 0.f)
    {
        float3 indirectIrradiance = SampleDDGI(
            positionWS,
            normalWS,
            DDGIGetSurfaceBias(normalWS,
                               viewDirWS,
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
            g_ProbeOffsetIndexTexIndex,
            g_RelocationLUTIndex,
            g_ProbeStatesIndex,
            WarpLinearSampler
            );
        float maxAlbedo = 0.9f;
        float3 indirectRadiance = min(baseColorAlpha.rgb, maxAlbedo) / PI * indirectIrradiance;

        indirectRadiance *= blendWeight;
        result += indirectRadiance;
    }

    GIDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex].Irradiance = result;
}