#include "private\SharedCommon.hlsli"

#pragma Vertex BlitVS
#pragma Pixel BlitPS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Disabled

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
    PSInput o = (PSInput) 0;
    
    if (vertexID == 0)
    {
        o.positionCS = float4(-1.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 0.0f);
    }
    else if (vertexID == 1)
    {
        o.positionCS = float4(3.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(2.0f, 0.0f);
    }
    else
    {
        o.positionCS = float4(-1.0f, -3.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 2.0f);
    }
    
    return o;
}

float4 BlitPS(PSInput i) : SV_TARGET
{
    Texture2D blitterTex = ResourceDescriptorHeap[blitterTextureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[ClampLinearSampler];
    
    half4 blitterValue = blitterTex.SampleLevel(linearSampler, i.uv, 0);
    
    return blitterValue;
}