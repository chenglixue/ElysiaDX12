#include "private\Common.hlsl"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend AlphaBlend
#pragma Depth Disabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_MaskSize;
    UINT g_MaskTexIndex;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VS(UINT vertexID : SV_VertexID)
{
    PSInput o;
    o.uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.positionCS = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

float2 SampleMask(float2 uv)
{
    Texture2D<float4> maskTex = ResourceDescriptorHeap[g_MaskTexIndex];
    SamplerState pointSampler = SamplerDescriptorHeap[ClampPointSampler];
    return maskTex.SampleLevel(pointSampler, uv, 0).rg;
}

float4 PS(PSInput i) : SV_TARGET
{
    float2 texel = max(g_MaskSize.zw, float2(1.0e-6f, 1.0e-6f));
    float2 center = SampleMask(i.uv);
    float2 grown = center;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
            grown = max(grown, SampleMask(i.uv + float2(x, y) * texel));
    }

    float visibleEdge = saturate(grown.r - center.r);
    float coverageEdge = saturate(grown.g - center.g);
    float occludedEdge = saturate(coverageEdge - visibleEdge);

    float3 visibleColor = float3(1.0f, 0.62f, 0.16f);
    float3 occludedColor = float3(0.55f, 0.34f, 0.12f);
    float alpha = max(visibleEdge, occludedEdge * 0.75f);
    float3 color = visibleEdge > 0.0f ? visibleColor : occludedColor;
    return float4(color, alpha);
}
