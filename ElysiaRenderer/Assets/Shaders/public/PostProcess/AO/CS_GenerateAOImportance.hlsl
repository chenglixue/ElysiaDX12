#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_DeinterleavedAOSize;
    float4 g_ImportanceBufferSize;
    Vector4 g_DeinterleaveAOTexIndices;
    UINT g_TargetTexIndex;
    float g_AOIntensityMul;
    float g_AOIntensityPow;

}

void Elysia_AOImportance_StoreOutput(UINT2 id, float value)
{
    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = value;
}

float Elysia_AOImportance_SampleImportance(float2 screenUV)
{
    return SampleTexture2D(g_TargetTexIndex, screenUV, ClampLinearSampler);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void GenerateAOImportance(UINT3 id : SV_DispatchThreadID)
{
    UINT2 basePos = id * 2;
    float2 AOUV = (basePos + 0.5f) * g_DeinterleavedAOSize.zw;
    float avg = 0.f;
    float minV = 1.f;
    float maxV = 0.f;

    [unroll]
    for (int i = 0; i < 4; i ++)
    {
        UINT layerIndex = i;
        float4 vals = GatherRedTexture2D(g_DeinterleaveAOTexIndices[layerIndex],
                                         AOUV,
                                         ClampPointSampler);

        //vals *= g_AOIntensityMul;
        vals = 1 - vals;

        //vals = pow(saturate(vals), g_AOIntensityPow);

        avg += dot(float4(vals.x, vals.y, vals.z, vals.w),
                   float4(1.0 / 16.0, 1.0 / 16.0, 1.0 / 16.0, 1.0 / 16.0));

        maxV = max(maxV, max(max(vals.x, vals.y), max(vals.z, vals.w)));
        minV = min(minV, min(min(vals.x, vals.y), min(vals.z, vals.w)));
    }

    float minMaxDiff = maxV - minV;

    Elysia_AOImportance_StoreOutput(id, pow(saturate(minMaxDiff * 2.0), 0.8));
}

static const float Smooth_Importance = 0.5f;

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void PostAOImportanceA(UINT3 id : SV_DispatchThreadID)
{
    float2 screenUV = (id + 0.5f) * g_ImportanceBufferSize.zw;
    float centerVal = Elysia_AOImportance_SampleImportance(screenUV);

    float2 halfTexel = 0.5f * g_ImportanceBufferSize.zw;
    float4 vals;
    vals[0] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(-halfTexel.x * 3, -halfTexel.y));
    vals[1] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(+halfTexel.x, -halfTexel.y * 3));
    vals[2] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(+halfTexel.x * 3, +halfTexel.y));
    vals[3] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(-halfTexel.x, +halfTexel.y * 3));

    float avgVal = dot(vals, 0.25f);
    vals.xy = max(vals.xy, vals.zw);
    float maxVal = max(centerVal, max(vals.x, vals.y));
    Elysia_AOImportance_StoreOutput(id, lerp(maxVal, avgVal, Smooth_Importance));
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void PostAOImportanceB(UINT3 id : SV_DispatchThreadID)
{
    float2 screenUV = (id + 0.5f) * g_ImportanceBufferSize.zw;
    float centerVal = Elysia_AOImportance_SampleImportance(screenUV);

    float2 halfTexel = 0.5f * g_ImportanceBufferSize.zw;
    float4 vals;
    vals[0] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(-halfTexel.x, -halfTexel.y * 3));
    vals[1] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(+halfTexel.x * 3, -halfTexel.y));
    vals[2] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(+halfTexel.x, +halfTexel.y * 3));
    vals[3] = Elysia_AOImportance_SampleImportance(
        screenUV + float2(-halfTexel.x * 3, +halfTexel.y));

    float avgVal = dot(vals, 0.25f);
    vals.xy = max(vals.xy, vals.zw);
    float maxVal = max(centerVal, max(vals.x, vals.y));
    Elysia_AOImportance_StoreOutput(id, lerp(maxVal, avgVal, Smooth_Importance));
}