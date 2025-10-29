#if EDITOR
#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\SharedCommon.hlsli>
#else
#include "../private\ShadingCommon.hlsl"
#include "../private\Light.hlsl"
#include "../private\LightCommon.hlsl"
#include "../private\SharedCommon.hlsli"
#endif

struct VSInput
{
    float3  positionOS   : POSITION;
    float2  uv           : TEXCOORD0;
    UINT    vertexID    : SV_VertexID;
};

struct PSInput
{
    float4  positionCS   : SV_POSITION;
    float2  uv           : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput) 0;

    o.positionCS = float4(
        i.vertexID <= 1 ? -1.0 : 3.0,
        i.vertexID == 1 ? 3.0 : -1.0,
        0.0, 1.0
    );

    o.uv = float2(
        i.vertexID <= 1 ? 0.0 : 2.0,
        i.vertexID == 1 ? 2.0 : 0.0
    );
    
    o.uv = i.uv;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    o.target0.rg = i.uv;
    
    return o;
}