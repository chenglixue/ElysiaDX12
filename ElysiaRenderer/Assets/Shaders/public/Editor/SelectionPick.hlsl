#include "private\Common.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer BackFaceCull
#pragma Blend Disabled
#pragma Depth Disabled

// UE-style HitProxy picking.
// Target is a R32_FLOAT RT; value = g_EntityID + 1, 0 = background.
// (R32_FLOAT instead of R32_UINT so the engine debug view can sample it.)
//
// The item index arrives through the per-pass constant buffer, which is
// re-uploaded and re-bound before every item draw.
// Depth visibility is resolved manually against the camera depth
// (same convention as SelectionMask.hlsl), so the PSO binds no depth target.
cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix g_WorldMatrix;
    Matrix g_ViewProjMatrix;
    float4 g_ScreenSize;
    UINT g_DepthTexIndex;
    UINT g_EntityID;
};

struct VSInput
{
    float3 positionOS : POSITION;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
};

PSInput VS(VSInput i)
{
    PSInput o;
    float4 positionWS = mul(float4(i.positionOS, 1.0f), g_WorldMatrix);
    o.positionCS = mul(positionWS, g_ViewProjMatrix);
    return o;
}

float PS(PSInput i) : SV_TARGET
{
    float2 screenSize = max(g_ScreenSize.xy, float2(1.0f, 1.0f));
    float2 uv = i.positionCS.xy / screenSize;

    Texture2D<float> depthTex = ResourceDescriptorHeap[g_DepthTexIndex];
    SamplerState pointSampler = SamplerDescriptorHeap[ClampPointSampler];
    float sceneDepth = depthTex.SampleLevel(pointSampler, uv, 0);
    float bias = max(0.0005f, sceneDepth * 0.002f);

    // Occluded fragments must be DISCARDED, not written as 0: a pixel keeps the
    // value of the last draw that wrote it, so writing 0 here would erase the id
    // of the front-most surface written by an earlier draw.
    if (i.positionCS.z > sceneDepth + bias)
        discard;

    return float(g_EntityID) + 1.0f;
}
