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

    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    UINT g_AOImportanceTexIndex;
    float g_BlendWeight;
    float g_Sharpness_Inv;
}

#define AO_GROUP_SIZE 8

float4 UnpackEdges(float _packedVal);
float Elysia_Sample_Importance(float2 uv);

[numthreads(AO_GROUP_SIZE, AO_GROUP_SIZE, 1)]
void TAA(uint3 id: SV_DispatchThreadID)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_TargetTexIndex];
    float2 screenUV = (float2(id.xy) + 0.5f) * g_TargetSize.zw;

    float rawDepth = LoadTexture2D(OpaqueDepthIndex, id);
    // float3 worldPos = ComputeWorldSpacePosition(screenUV, rawDepth, viewProjMatrix_I);
    //
    // float4 preClipPos = mul(float4(worldPos, 1.f), pre_viewProjMatrix);
    // preClipPos /= preClipPos.w;
    //
    // float2 preScreenUV = preClipPos.xy * 0.5f * float2(1.f, -1.f) + 0.5f;
    // if (any(preScreenUV < 0.f) || any(preScreenUV > 1.f))
    // {
    //     o[id.xy] = LoadTexture2D(g_AOIndex, id);
    //     return;
    // }
    //
    // float2 velocity = screenUV - preScreenUV;
    float2 velocity = SampleTexture2D(GBuffer5Index, screenUV, ClampPointSampler);
    float2 preScreenUV = screenUV - velocity;
    if (rawDepth >= 0.999f)
    {
        preScreenUV = screenUV;
    }

    float4 historyAO = SampleTexture2D(g_SourceTexIndex, preScreenUV, ClampLinearSampler);
    float4 currAO = SampleTexture2D(g_AOIndex, screenUV, ClampLinearSampler);

    float4 m1 = 0.0f;
    float4 m2 = 0.0f;

    float4 edgesLRTB = UnpackEdges(currAO.y);
    float edgeScore = dot(edgesLRTB, 0.25f);

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            int2 pos = id.xy + int2(x, y);
            float4 neighbor = LoadTexture2D(g_AOIndex, pos);

            m1 += neighbor;
            m2 += neighbor * neighbor;
        }
    }

    float4 mu = m1 / 9.0f;
    float4 sigma = sqrt(abs(m2 / 9.0f - mu * mu));

    float Gamma = 2.0f;

    float4 p_clip = historyAO;
    float4 e_clip = mu;
    float4 d_clip = p_clip - e_clip;
    float4 r_clip = max(abs(d_clip / max(sigma * Gamma, 0.00001f)), 1.0f);
    historyAO = e_clip + d_clip / r_clip;

    float dynamicWeight = lerp(g_BlendWeight, g_BlendWeight * 2.0, 1.0 - edgeScore);

    float4 finalAO = lerp(historyAO, currAO, dynamicWeight);
    o[id.xy] = finalAO;
}

float4 UnpackEdges(float _packedVal)
{
    uint packedVal = (uint)(_packedVal * 255.5);
    float4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_Sharpness_Inv);
}

float Elysia_Sample_Importance(float2 uv)
{
    return SampleTexture2D(g_AOImportanceTexIndex, uv, ClampLinearSampler);
}