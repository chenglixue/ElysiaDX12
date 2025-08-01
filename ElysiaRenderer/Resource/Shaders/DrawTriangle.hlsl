struct VSInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    //float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    //float4 color : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o;

    o.position = i.position;
    o.texcoord = i.texcoord;
    //result.color = i.color;

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o;
    
    o.target0.rg = i.texcoord;
    return o;
}