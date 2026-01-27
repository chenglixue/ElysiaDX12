#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    UINT g_TargetTexIndex;
    float g_DepthImportanceThreshold;
    float g_NormalImportanceThreshold;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void GenerateAOImportance(UINT3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    float2 screenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;
    float centerRawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler).r;
    float centerEyeDepth = LinearEyeDepth(centerRawDepth, g_ZBufferParams);
    //float3 centerNormal = SampleNormalWS(screenUV, ClampPointSampler);

    float2 texelOffsets[4] =
    {
        float2(g_TargetSize.z, 0),
        float2(-g_TargetSize.z, 0),
        float2(0, g_TargetSize.w),
        float2(0, -g_TargetSize.w)
    };

    float eyeDepths[4];
    float3 eyeNormal[4];
    float depthDiff = 0.f;
    float normalDiff = 0.f;
    [unroll(4)]
    for (int i = 0; i < 4; i ++)
    {
        eyeDepths[i] = SampleTexture2D(OpaqueDepthIndex, screenUV + texelOffsets[i], ClampPointSampler);
        eyeDepths[i] = LinearEyeDepth(eyeDepths[i], g_ZBufferParams);
        //eyeNormal[i] = SampleNormalWS(screenUV + texelOffsets[i], ClampPointSampler);

        depthDiff += abs(centerEyeDepth - eyeDepths[i]);
        // float normalDot = dot(eyeNormal[i], centerNormal);
        // normalDiff += 1 - normalDot;
    }
    depthDiff /= max(centerEyeDepth, 1e-4);

    float depthImportance = smoothstep(g_DepthImportanceThreshold * 0.5f, g_DepthImportanceThreshold, depthDiff);
    //float normalImportance = smoothstep(g_NormalImportanceThreshold * 0.5f, g_NormalImportanceThreshold, normalDiff);
    float importance = depthImportance;

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];
    o[id.xy] = importance;
}