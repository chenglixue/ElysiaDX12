struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput result;

    result.position = i.position;
    result.color = i.color;

    return result;
}

PSOutput PS(PSInput i)
{
    PSOutput o;
    
    o.target0 = i.color;
    return o;
}