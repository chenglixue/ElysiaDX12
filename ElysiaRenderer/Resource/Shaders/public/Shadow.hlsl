#if defined(EDITOR)
    #include <private\ShadingCommon.hlsl>

    #include <private\Light.hlsl>
    #include <private\LightCommon.hlsl>
#else
    #include "../private\ShadingCommon.hlsl"

    #include "../private\Light.hlsl"
    #include "../private\LightCommon.hlsl"
#endif

struct VSInput
{
    float3 positionOS : POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normalOS : NORMAL;
    float3 tangentOS : TANGENT;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float4 positionVS : VIEW_POSITION;
    float4 positionWS : WORLD_POSITION;
    float3 normalWS : NORMAL;
    float3 tangentWS : TANGENT;
    float3 bitTangentWS : BITTANGENT;
    float2 uv : TEXCOORD;
    float3 color : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput) 0;

    o.positionWS = mul(float4(i.positionOS, 1.f), M_World);
    o.positionVS = mul(o.positionWS, M_ShadowView);
    o.positionCS = mul(o.positionVS, M_ShadowProj);
    
    bool hasTangent = true;
    if (hasTangent)
    {
        float3 N = normalize(mul(i.normalOS, (float3x3) M_World));
        float3 T = mul(i.tangentOS, (float3x3) M_World);
        
        o.tangentWS = normalize(T - dot(N, T) * N);
        o.bitTangentWS = (cross(o.tangentWS, N));
        o.normalWS = N;
    }
    else
    {
        o.normalWS = normalize(mul(i.normalOS, (float3x3) M_World));
    }
    
    o.uv = i.uv;
    o.color = i.color;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    return o;
}