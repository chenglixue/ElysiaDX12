#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    Vector4 g_DeinterleaveAOTexIndices;
    UINT g_TargetTexIndex;
    float g_ImportanceIntensity;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void GenerateAOImportance(UINT3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    float sliceAO[4];
    sliceAO[0] = LoadTexture2D(g_DeinterleaveAOTexIndices[0], id);
    sliceAO[1] = LoadTexture2D(g_DeinterleaveAOTexIndices[1], id);
    sliceAO[2] = LoadTexture2D(g_DeinterleaveAOTexIndices[2], id);
    sliceAO[3] = LoadTexture2D(g_DeinterleaveAOTexIndices[3], id);

    float maxAO = FLT_MIN;
    float minAO = FLT_MAX;
    [unroll(4)]
    for (int i = 0; i < 4; ++i)
    {
        maxAO = max(maxAO, sliceAO[i]);
        minAO = min(minAO, sliceAO[i]);
    }

    float diff = abs(maxAO - minAO);
    float importance = saturate(diff * g_ImportanceIntensity);

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = importance;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void MaxAOImportance(UINT3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    float2 centerUV = (id.xy + 0.5f) * g_TargetSize.zw;
    float center = LoadTexture2D(g_TargetTexIndex, id);

    float2 offsetUV[4] =
    {
        float2(-1.5f, -0.5f),
        float2(0.5, -1.5),
        float2(1.5, 0.5),
        float2(-0.5, 1.5),
    };

    float maxImportance = center;
    for (int i = 0; i < 4; ++i)
    {
        float sample = LoadTexture2D(g_TargetTexIndex, id.xy + offsetUV[i]);
        maxImportance = max(maxImportance, sample);
    }

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = maxImportance;
}