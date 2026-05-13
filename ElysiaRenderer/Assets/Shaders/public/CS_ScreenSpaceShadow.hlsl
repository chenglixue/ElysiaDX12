#include "private\ShadingCommon.hlsl"
#include <private\Light.hlsl>
#include <private\TAACommon.hlsli>

#define GROUP_SIZE 8

#pragma shader_feature SHADOW_QUALITY_LOW SHADOW_QUALITY_MIDDLE SHADOW_QUALITY_HIGH SHADOW_QUALITY_VERYHIGH
#pragma shader_feature HARD_SHADOW SOFT_SHADOW

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_ShadowMaskTexSize;

    UINT g_ShadowMaskTexIndex;
    UINT g_HistoryTexIndex;
    UINT g_CurrTexIndex;
    UINT g_SobolNoiseTexIndex;

    float g_StaticBlendWeight;
    float g_DynamicBlendWeight;
    float g_MaxBlendWeight;
    float2 g_SobolSequence[64];
}

#include <private\ShadowCommon.hlsl>


void SaveShadowMask(UINT2 writePos, float3 shadow)
{
    RWTexture2D<float3> ShadowMaskTex = ResourceDescriptorHeap[g_ShadowMaskTexIndex];
    ShadowMaskTex[writePos].rgb = shadow;
}

float LoadSceneRawDepth(UINT2 readPos)
{
    float rawDepth = LoadTexture2D(OpaqueDepthIndex, readPos);
    return rawDepth;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ScreenSpaceShadow(uint3 GlobalID : SV_DispatchThreadID)
{
    UINT2 readPos = GlobalID.xy;
    UINT2 writePos = readPos;
    float2 screenSize = g_ShadowMaskTexSize.xy;

    if (readPos.x >= g_ShadowMaskTexSize.x || readPos.y >= g_ShadowMaskTexSize.y)
        return;

    float2 screenUV = ((float2)readPos + 0.5f) * g_ShadowMaskTexSize.zw;
    float sceneRawDepth = LoadSceneRawDepth(readPos);
    float3 positionWS = ComputeWorldSpacePosition(screenUV, sceneRawDepth, viewProjMatrix_I);

    float shadow = SunShadowVisibility(positionWS,
                                       screenUV,
                                       screenSize,
                                       g_ShadowRadius,
                                       shadowSize,
                                       shadowMatrix,
                                       g_SobolSequence);

    SaveShadowMask(writePos, shadow);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ShadowTAA(uint3 GlobalID : SV_DispatchThreadID)
{
    UINT2 readPos = GlobalID.xy;
    UINT2 writePos = GlobalID.xy;
    if (writePos.x >= g_ShadowMaskTexSize.x || writePos.y >= g_ShadowMaskTexSize.y)
        return;

    float2 screenUV = ((float2)readPos + 0.5f) * g_ShadowMaskTexSize.zw;
    float2 closetUV = SampleClosestUV3x3(OpaqueDepthIndex, screenUV, g_ShadowMaskTexSize.zw);

    float currShadow = SampleTexture2D(g_CurrTexIndex, screenUV, ClampLinearSampler).r;

    float2 velocity = Elysia_Sample_Velocity(closetUV);
    float2 preUV = screenUV - velocity;
    if (any(preUV < 0.f) || any(preUV > 1.f))
    {
        Elysia_Save_TAA(g_CurrTexIndex, writePos, currShadow);
        return;
    }

    float historyShadow = SampleTexture2D(g_HistoryTexIndex, preUV, ClampLinearSampler);

    float m1 = 0.0f;
    float m2 = 0.0f;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            int2 pos = readPos + int2(x, y);
            float neighbor = LoadTexture2D(g_CurrTexIndex, pos);

            m1 += neighbor;
            m2 += neighbor * neighbor;
        }
    }
    float mu = m1 / 9.0f;
    float sigma = sqrt(abs(m2 / 9.0f - mu * mu));
    float Gamma = 1.0f;
    float p_clip = historyShadow;
    float e_clip = mu;
    float d_clip = p_clip - e_clip;
    float r_clip = max(abs(d_clip / max(sigma * Gamma, 0.00001f)), 1.0f);
    historyShadow = e_clip + d_clip / r_clip;

    float velocityFactor = length(velocity) * g_ShadowMaskTexSize.xy;
    float blendWeight = CalcTAAWeight(g_StaticBlendWeight, g_DynamicBlendWeight, g_MaxBlendWeight, velocityFactor);

    float blendColor = lerp(currShadow, historyShadow, blendWeight);
    Elysia_Save_TAA(g_CurrTexIndex, writePos, blendColor);
}