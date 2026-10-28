#include "private\Common.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer Wireframe
#pragma Blend Disabled
#pragma Depth Disabled

// Drawn after tonemap, on top of the lit image. FILL_WIREFRAME rasterizes
// triangle edges only. The depth test is done here so hidden edges drop out
// without a depth target. UE static-mesh wire color is (0, 255, 255); this
// renderer has no mobility, so every mesh uses that color.
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
    float bias = max(0.0008f, sceneDepth * 0.003f);
    if (i.positionCS.z > sceneDepth + bias)
        discard;

    return float4(0.0f, 1.0f, 1.0f, 1.0f);
}
