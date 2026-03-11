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
    UINT compactedIndex = id.x;

    StructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];
    RWStructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
    StructuredBuffer<CompactedRay> CompactedRayDataBuffer = ResourceDescriptorHeap[g_CompactedRayBufferIndex];
    StructuredBuffer<UINT> CompactedRayIndexBuffer = ResourceDescriptorHeap[g_CompactedIndicesBufferIndex];
    Texture2D<float3> BlueNoiseTex = ResourceDescriptorHeap[BlueNoiseTexIndex];

    if (compactedIndex >= globalCounter[0])
        return;

    UINT originIndex = CompactedRayIndexBuffer[compactedIndex];
    UINT probeIndex = originIndex / RAYS_PER_PROBE;
    UINT rayIndex = originIndex % RAYS_PER_PROBE;

    CompactedRay rayData = CompactedRayDataBuffer[compactedIndex];
    float distance = rayData.Position.w;

    UINT baseColorTexIndex = rayData.Data.g;
    UINT normalTexIndex = rayData.Data.b;
    float pdf = rayData.Data.a;
    float3 normalWS = UnpackNormal(rayData.Position.r);
    float3 positionWS = rayData.Position.xyz;
    float4 positionVS = mul(float4(positionWS, 1.f), viewMatrix);
    float4 positionCS = mul(positionVS, projMatrix);
    float2 positionNDC = positionCS * rcp(positionCS.w);
    float2 uv = positionNDC * float2(1.f, -1.f) * 0.5f + 0.5f;
    float3 baseColorAlpha = SampleTexture2D(baseColorTexIndex, uv, WarpLinearSampler);

    const float SHORT_DISTANCE_THRESHOLD = 10.f;
    const bool needRT = distance <= SHORT_DISTANCE_THRESHOLD;

    float3 result = 0.f;
    float blendWeight = DDGIGetVolumeBlendWeight(positionWS, g_GridOrigin, g_GridSpacing, 0, float4(0, 0, 0, 1));
    Vector3 viewDirWS = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    if (needRT)
    {
        LightData mainLightData = GetMainLight(mainLight);
        float3 toLight = normalize(mainLightData.toLight);
        float shadowSpread = 0.05f;
        float3 jitteredToLight = GetJitteredDirection(toLight,
                                                      shadowSpread,
                                                      probeIndex,
                                                      rayIndex,
                                                      frameIndex,
                                                      BlueNoiseTex);
        float NoL = max(0, dot(normalWS, jitteredToLight));

        float shadow = DDGI_Query_Shadow_Visibity(positionWS,
                                                  normalWS,
                                                  g_ProbeNormalBias,
                                                  jitteredToLight,
                                                  g_SceneTLAS);

        float3 directIrradiance = baseColorAlpha.rgb / PI * mainLightData.color * mainLightData.intensity * NoL *
                                  shadow;
        result = directIrradiance;

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
            result += (indirectRadiance);

        }
        result = shadow;
    }
    else
    {
        if (blendWeight > 0.f)
        {
            float3 cacheIrradiance = SampleDDGI(positionWS,
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
                                                WarpLinearSampler);
            result += (baseColorAlpha.rgb / PI * cacheIrradiance);
        }
    }

    GIData data = (GIData)0;
    data.Irradiance = saturate(result) * rcp(pdf);
    data.Distance = distance;
    GIDataBuffer[originIndex] = data;
}