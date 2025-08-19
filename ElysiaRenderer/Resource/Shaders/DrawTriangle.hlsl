#define perObjectSpace   space0
#define perMaterialSpace space1
#define perPassSpace     space2
#define perFrameSpace    space3

SamplerState g_Sampler_WarpU_WarpV_Point : register(s0);
SamplerState g_Sampler_ClampU_ClampV_Point : register(s1);
SamplerState g_Sampler_WarpU_WarpV_Linear : register(s2);
SamplerState g_Sampler_ClampU_ClampV_Linear : register(s3);
SamplerState g_Sampler_WarpU_WarpV_Anisotropic : register(s4);
SamplerState g_Sampler_ClampU_ClampV_Anisotropic : register(s5);

cbuffer PerPassBuffer : register(b0, perPassSpace)
{
    float4      CameraPosWS;
    float4x4    M_View;
    float4x4    M_Proj;
    //float4 padding[16];
}
cbuffer PerObjectBuffer : register(b0, perObjectSpace)
{
    float4x4 M_World;
    //float4 padding[16];
}

Texture2D g_texture : register(t0, perObjectSpace);

struct VSInput
{
    float3 positionOS   : POSITION;
    //float3 color        : COLOR;
    //float2 uv           : TEXCOORD0;
    //float3 normalOS     : NORMAL;
    //float3 tangentOS    : TANGENT;
};

struct PSInput
{
    float4 positionCS   : SV_POSITION;
    float4 positionVS   : VIEW_POSITION;
    float4 positionWS   : WORLD_POSITION;
    float4 normalWS     : NORMAL;
    float2 uv           : TEXCOORD;
    float3 color        : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

float3 Rand3d(float3 pos)
{
    float3 t = frac(sin(dot(pos, float3(12.9898, 45.164, 78.233))) * 43758.5453123);
    return t;
}

PSInput VS(VSInput i)
{
    PSInput o;

    o.positionWS = mul(M_World, float4(i.positionOS, 1.f));
    o.positionVS = mul(M_View, o.positionWS);
    o.positionCS = mul(M_Proj, o.positionVS);
    //o.uv = i.uv;
    //o.color = i.color;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;
    
    o.target0.rgb = 1;
    return o;
}