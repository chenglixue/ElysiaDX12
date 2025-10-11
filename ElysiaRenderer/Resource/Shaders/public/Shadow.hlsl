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
    float3 uv : TEXCOORD0;
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

    o.positionWS = mul(worldMatrix, float4(i.positionOS, 1.f));
    o.positionCS = mul(shadowMatrix, o.positionWS);
    
    o.uv = i.uv;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    if (baseColorTexIndex != -1)
    {
        SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
        Texture2D<float4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
        
        float4 baseColor = baseColorTex.Sample(warpLinearSampler, i.uv)
            * float4(baseColorTint, opacity);
        clip(baseColor.a - cutoff);

    }
    
    return o;
}