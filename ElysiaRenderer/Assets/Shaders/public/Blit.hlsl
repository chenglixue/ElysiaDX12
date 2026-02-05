#include "private\ShadingCommon.hlsl"

#pragma Vertex BlitVS
#pragma Pixel BlitPS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Enabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT blitterTextureIndex;
}

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput BlitVS(UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput)0;

    o.uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.positionCS = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);

    // if (vertexID == 0)
    // {
    //     o.positionCS = float4(-1.0f, 1.0f, 1.0f, 1.0f);
    //     o.uv = float2(0.0f, 0.0f);
    // }
    // else if (vertexID == 1)
    // {
    //     o.positionCS = float4(3.0f, 1.0f, 0.0f, 1.0f);
    //     o.uv = float2(2.0f, 0.0f);
    // }
    // else
    // {
    //     o.positionCS = float4(-1.0f, -3.0f, 0.0f, 1.0f);
    //     o.uv = float2(0.0f, 2.0f);
    // }

    return o;
}

float4 BlitPS(PSInput i) : SV_TARGET
{
    half4 blitterValue = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);

    return blitterValue;
}