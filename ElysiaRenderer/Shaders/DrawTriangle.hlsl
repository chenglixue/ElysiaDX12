struct VSInput
{
    float4 position : POSITIONT;
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

PSInput VS(float4 position : POSITION, float4 color : COLOR)
{
    PSInput o;
    
    o.position = i.position;
    o.color = i.color;
    
    return o;
}

float4 PS(PSInput i) : SV_TARGET
{
    
    return i.color;
}

