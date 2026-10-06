#include "private\Common.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer BackFaceCull
#pragma Blend Disabled
#pragma Depth Disabled

// R is the visible surface, G is the full projected coverage.
// Cull back faces the same way GBuffer does. A closed mesh's back faces fail the
// depth compare, and with depth testing disabled they would overwrite the front
// face mask and turn every triangle edge into an outline.
//
// Per-item data (the world matrix) arrives through the per-pass constant buffer,
// which is re-uploaded and re-bound before every item draw.
cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix g_WorldMatrix;
    Matrix g_ViewProjMatrix;
    float4 g_ScreenSize;
    UINT g_DepthTexIndex;
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

float4 PS(PSInput i) : SV_TARGET
{
    float2 screenSize = max(g_ScreenSize.xy, float2(1.0f, 1.0f));
    float2 uv = i.positionCS.xy / screenSize;

    Texture2D<float> depthTex = ResourceDescriptorHeap[g_DepthTexIndex];
    SamplerState pointSampler = SamplerDescriptorHeap[ClampPointSampler];
    float sceneDepth = depthTex.SampleLevel(pointSampler, uv, 0);
    float bias = max(0.0005f, sceneDepth * 0.002f);
    float visible = i.positionCS.z <= sceneDepth + bias ? 1.0f : 0.0f;
    return float4(visible, 1.0f, 0.0f, 1.0f);
}
