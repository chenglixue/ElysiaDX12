#include "private\ShadingCommon.hlsl"
#include "private/SSAOCommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float4 g_TargetSize;
    float4 g_SourceSize;

    float g_MipmapLevel;
    UINT g_HIZMipmapCount;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AOHIZNormal(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 srcBaseCoord = dispatchThreadID.xy * 2;
    const uint2 destCoord = dispatchThreadID.xy;

    if (destCoord.x >= (uint)g_TargetSize.x || destCoord.y >= (uint)g_TargetSize.y)
        return;

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];

    float4 result;
    if (g_MipmapLevel == 0)
    {
        float2 screenUV = (float2(dispatchThreadID.xy) + 0.5f) * g_TargetSize.zw;
        float3 normalWS = EncodeNormal(SampleNormalWS(screenUV,ClampPointSampler));
        float rawDepth = SampleTexture2D(OpaqueDepthIndex, screenUV, ClampPointSampler);
        float eyeDepth = LinearEyeDepth(rawDepth, g_ZBufferParams);
        eyeDepth /= Constant_Float16F_Scale;

        result = float4(normalWS, eyeDepth);
    }
    else
    {
        const uint2 maxCoord = (uint2)g_SourceSize.xy - 1;

        uint2 sampleUV[4];
        sampleUV[0] = min(srcBaseCoord + uint2(0, 0), maxCoord);
        sampleUV[1] = min(srcBaseCoord + uint2(0, 1), maxCoord);
        sampleUV[2] = min(srcBaseCoord + uint2(1, 0), maxCoord);
        sampleUV[3] = min(srcBaseCoord + uint2(1, 1), maxCoord);

        float4 samples[4];
        samples[0] = LoadTexture2D(g_SourceTexIndex, sampleUV[0]);
        samples[1] = LoadTexture2D(g_SourceTexIndex, sampleUV[1]);
        samples[2] = LoadTexture2D(g_SourceTexIndex, sampleUV[2]);
        samples[3] = LoadTexture2D(g_SourceTexIndex, sampleUV[3]);

        float minDepth = FLT_MAX;
        [unroll(4)]
        for (int i = 0; i < 4; i ++)
        {
            minDepth = min(minDepth, samples[i].a);
        }

        float3 weightedNormalSum = 1e-4;
        if (g_MipmapLevel <= 2)
        {
            float totalWeight = 1e-4;
            float weights[4];
            [unroll(4)]
            for (int i = 0; i < 4; i ++)
            {
                weights[i] = ComputeDepthSimilarity(samples[i].a, minDepth);
                totalWeight += weights[i];
                weightedNormalSum += samples[i].rgb * weights[i];
            }
            weightedNormalSum /= totalWeight;
        }

        result = float4(weightedNormalSum, minDepth);
    }

    o[destCoord] = result;
}