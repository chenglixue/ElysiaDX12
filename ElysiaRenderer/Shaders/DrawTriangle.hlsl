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

cbuffer SceneParameterBuffer : register(b0, space2)
{
    float4 offset;
    float4 padding[15];
}

Texture2D g_texture : register(t0);

struct VSInput
{
    float4 position : POSITIONT;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o;

    o.position = i.position;
    o.uv = i.uv;

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o;
    
    o.target0 = i.position;
    //o.target0.rg = i.uv;
    //o.target0 = g_texture.Sample(g_Sampler_WarpU_WarpV_Linear, i.uv, 0);
    return o;
}