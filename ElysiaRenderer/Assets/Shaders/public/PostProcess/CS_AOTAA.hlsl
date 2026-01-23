#include "private\SSAOCommon.hlsli"

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    Matrix pre_viewMatrix;
    Matrix pre_viewMatrix_I;
    Matrix pre_projMatrix;
    Matrix pre_projMatrix_I;
    Matrix pre_viewProjMatrix;
    Matrix pre_viewProjMatrix_I;

    Vector4 g_TargetSize;
    Vector4 g_SourceSize;

    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float g_BlendWeight;
}

#define AO_GROUP_SIZE 8

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void TAA(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];
    float2 screenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;

    float rawDepth = LoadTexture2D(OpaqueDepthIndex, id);
    float3 worldPos = ComputeWorldSpacePosition(screenUV, rawDepth, viewProjMatrix_I);

    float4 preClipPos = mul(float4(worldPos, 1.f), pre_viewProjMatrix);
    preClipPos /= preClipPos.w; // 透视除法

    float2 preScreenUV = preClipPos.xy * 0.5f * float2(1.f, -1.f) + 0.5f;
    if (any(preScreenUV < 0.f) || any(preScreenUV > 1.f))
    {
        o[id.xy] = LoadTexture2D(g_AOIndex, id);
        return;
    }

    float2 velocity = screenUV - preScreenUV;
    if (rawDepth >= 0.999f)
        velocity = 0;

    float4 historyAO = SampleTexture2D(g_SourceTexIndex, preScreenUV, ClampLinearSampler);
    float4 currAO = SampleTexture2D(g_AOIndex, screenUV, ClampLinearSampler);

    float4 m1 = 0.0f; // 一阶矩 (Mean)
    float4 m2 = 0.0f; // 二阶矩 (Mean of Squares)

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            int2 pos = id.xy + int2(x, y);
            float4 neighbor = LoadTexture2D(g_AOIndex, pos); // 读取当前帧

            m1 += neighbor;
            m2 += neighbor * neighbor;
        }
    }

    // 计算平均值和方差
    float4 mu = m1 / 9.0f;
    float4 sigma = sqrt(abs(m2 / 9.0f - mu * mu));

    // 构建包围盒 (AABB)
    // Gamma 是包围盒大小，通常 1.0 ~ 1.5。越大越稳定但拖影越多，越小噪点越多。
    float Gamma = 1.0f;
    float4 minAO = mu - Gamma * sigma;
    float4 maxAO = mu + Gamma * sigma;

    historyAO = clamp(historyAO, minAO, maxAO);
    float4 finalAO = lerp(historyAO, currAO, g_BlendWeight);
    o[id.xy] = finalAO;
}