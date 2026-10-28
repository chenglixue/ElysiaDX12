#include "private\ShadingCommon.hlsl"
#include "private\MonteCarlo.hlsl"
#include "private\Random.hlsl"

// Quality 1 of SSRTDiffuseIndirect.usf: 4 cosine rays, 8 steps.
// The march itself is not UE's Furthest HZB CastScreenSpaceRay. See Docs/UE5-SSGI.md.
#define GROUP_SIZE 8
#define SSGI_RAY_COUNT 4
#define SSGI_RAY_STEPS 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix projMatrix;
    Matrix projMatrix_I;
    float4 g_FullSize;
    float4 g_HalfSize;
    float g_MaxDistance;
    float g_Thickness;
    float g_SelfBias;
    float g_Intensity;
    float g_HistoryWeight;
    uint g_FrameIndex;
    uint g_RadianceHistorySRV;
    uint g_RadianceHistoryUAV;
    uint g_RawIndirectSRV;
    uint g_RawIndirectUAV;
    uint g_IndirectHistorySRV;
    uint g_IndirectDestSRV;
    uint g_IndirectDestUAV;
    uint g_CameraColorSRV;
    uint g_CameraColorUAV;
    uint g_PreExposureBufferIndex;
    uint g_PreExposureEnable;
    uint g_HasRadianceHistory;
    uint g_HasIndirectHistory;
}

float SSGILuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float HistoryPreExposureScale()
{
    if (g_PreExposureEnable == 0)
        return 1.0f;
    RWStructuredBuffer<float2> preExposure = ResourceDescriptorHeap[g_PreExposureBufferIndex];
    float historyPre = max(preExposure[0].r, 1e-5f);
    float currentPre = max(preExposure[0].g, 1e-5f);
    return currentPre / historyPre;
}

float2 ProjectViewToUV(float3 positionVS)
{
    float4 clip = mul(float4(positionVS, 1.0f), projMatrix);
    float2 ndc = clip.xy / max(clip.w, 1e-5f);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

bool IsFarDepth(float rawDepth)
{
    float eye = LinearEyeDepth(rawDepth, g_ZBufferParams);
    float farEye = LinearEyeDepth(1.0f, g_ZBufferParams);
    return eye >= farEye * 0.98f;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void Trace(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> raw = ResourceDescriptorHeap[g_RawIndirectUAV];
    if (id.x >= g_HalfSize.x || id.y >= g_HalfSize.y)
        return;

    float2 uv = ((float2)id.xy + 0.5f) * g_HalfSize.zw;
    float rawDepth = SampleTexture2D(OpaqueDepthIndex, uv, ClampPointSampler).r;
    FDecodeGBufferData gbuffer = GetDecodeGBufferData(uv);
    if (g_HasRadianceHistory == 0 ||
        gbuffer.ShadingModelID == Shading_Model_ID_Unlit ||
        IsFarDepth(rawDepth))
    {
        raw[id.xy] = 0;
        return;
    }

    float3 originVS = ComputeViewSpacePosition(uv, rawDepth, projMatrix_I);
    float3 normalWS = gbuffer.WorldNormal;
    float3 normalVS = normalize(mul(float4(normalWS, 0.0f), viewMatrix).xyz);
    if (dot(normalVS, normalVS) < 0.5f)
    {
        raw[id.xy] = 0;
        return;
    }

    uint2 seed = Rand3DPCG16(int3(int2(id.xy), int(g_FrameIndex % 8))).xy;
    float exposureScale = HistoryPreExposureScale();
    float3 weightedColor = 0;
    float hitCount = 0;

    [unroll]
    for (uint rayId = 0; rayId < SSGI_RAY_COUNT; ++rayId)
    {
        float2 E = Hammersley16(rayId, SSGI_RAY_COUNT, seed);
        float3 directionVS = normalize(TangentToWorld(CosineSampleHemisphereConcentric(E).xyz, normalVS));
        float maxDistance = max(g_MaxDistance, 0.1f);
        float3 endVS = originVS + directionVS * maxDistance;

        float previousGap = -1.0f;
        float3 previousRayVS = originVS;
        bool hit = false;
        float2 hitUV = uv;

        [loop]
        for (uint step = 1; step <= SSGI_RAY_STEPS; ++step)
        {
            float t = (float)step / (float)SSGI_RAY_STEPS;
            float3 rayVS = lerp(originVS, endVS, t);
            float4 clip = mul(float4(rayVS, 1.0f), projMatrix);
            if (clip.w <= 1e-4f)
                break;

            float2 sampleUV = ProjectViewToUV(rayVS);
            if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
                break;

            float sceneDepth = SampleTexture2D(OpaqueDepthIndex, sampleUV, ClampPointSampler).r;
            if (IsFarDepth(sceneDepth))
            {
                previousRayVS = rayVS;
                previousGap = -1.0f;
                continue;
            }

            float3 sceneVS = ComputeViewSpacePosition(sampleUV, sceneDepth, projMatrix_I);
            float gap = rayVS.z - sceneVS.z;
            float stepZ = abs(rayVS.z - previousRayVS.z);
            float accept = max(g_Thickness, stepZ) + g_SelfBias;
            if (previousGap <= g_SelfBias && gap > g_SelfBias && gap < accept)
            {
                hit = true;
                hitUV = sampleUV;
                break;
            }

            previousGap = gap;
            previousRayVS = rayVS;
        }

        if (hit)
        {
            float3 sampleColor = SampleTexture2D(g_RadianceHistorySRV, hitUV, ClampPointSampler).rgb;
            sampleColor = max(sampleColor, 0.0f) * exposureScale;
            float weight = rcp(1.0f + SSGILuminance(sampleColor));
            weightedColor += sampleColor * weight;
            hitCount += 1.0f;
        }
    }

    weightedColor *= rcp((float)SSGI_RAY_COUNT);
    float lum = SSGILuminance(weightedColor);
    weightedColor *= rcp(max(1.0f - lum, 1e-3f));
    raw[id.xy] = float4(max(weightedColor, 0.0f), hitCount * rcp((float)SSGI_RAY_COUNT));
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void Temporal(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_HalfSize.x || id.y >= g_HalfSize.y)
        return;

    float4 current = LoadTexture2D(g_RawIndirectSRV, int2(id.xy));
    float4 resolved = current;
    if (g_HasIndirectHistory != 0)
    {
        float2 uv = ((float2)id.xy + 0.5f) * g_HalfSize.zw;
        float2 historyUV = uv - SampleTexture2D(GBuffer5Index, uv, ClampPointSampler).xy;
        if (all(historyUV >= 0.0f) && all(historyUV <= 1.0f))
        {
            float4 history = SampleTexture2D(g_IndirectHistorySRV, historyUV, ClampLinearSampler);
            float keep = saturate(g_HistoryWeight);
            resolved.rgb = lerp(current.rgb, history.rgb, keep);
            resolved.a = lerp(current.a, history.a, keep);
        }
    }

    RWTexture2D<float4> dest = ResourceDescriptorHeap[g_IndirectDestUAV];
    dest[id.xy] = resolved;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void Composite(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_FullSize.x || id.y >= g_FullSize.y)
        return;

    float2 uv = ((float2)id.xy + 0.5f) * g_FullSize.zw;
    float rawDepth = LoadTexture2D(OpaqueDepthIndex, int2(id.xy)).r;
    FDecodeGBufferData gbuffer = GetDecodeGBufferData(uv);
    if (gbuffer.ShadingModelID == Shading_Model_ID_Unlit || IsFarDepth(rawDepth))
        return;

    float3 indirect = SampleTexture2D(g_IndirectDestSRV, uv, ClampLinearSampler).rgb;
    float3 add = max(indirect, 0.0f) * gbuffer.DiffuseColor * max(g_Intensity, 0.0f);

    RWTexture2D<float4> color = ResourceDescriptorHeap[g_CameraColorUAV];
    float4 scene = color[id.xy];
    scene.rgb += add;
    color[id.xy] = scene;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CopyHistory(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_FullSize.x || id.y >= g_FullSize.y)
        return;

    float4 scene = LoadTexture2D(g_CameraColorSRV, int2(id.xy));
    RWTexture2D<float4> history = ResourceDescriptorHeap[g_RadianceHistoryUAV];
    history[id.xy] = scene;
}
