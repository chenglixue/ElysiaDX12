#if EDITOR
#include <private\SharedCommon.hlsli>
#else
#include "../private\SharedCommon.hlsli"
#endif

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VS(UINT vertexID : SV_VertexID)
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