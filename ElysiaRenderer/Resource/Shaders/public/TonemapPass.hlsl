#if EDITOR
#include <public\Blit.hlsl>
#include <private\Color.hlsl>
#else
#include "Blit.hlsl"
#include "../private\Color.hlsl"
#endif

struct PSOutput
{
    float4 target0 : SV_TARGET0;
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

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    Texture2D blitterTex = ResourceDescriptorHeap[blitterTextureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[ClampLinearSampler];
    
    float4 blitterValue = blitterTex.SampleLevel(linearSampler, i.uv, 0);
    
    float4 linearColor = GetSRGBToLinear(blitterValue);
    //linearColor = blitterValue;
    
    float3 tonemapColor = NeutralTonemap(linearColor.rgb);
    tonemapColor = GetLinearToSRGB(tonemapColor);
    
    o.target0 = float4(tonemapColor, 1.f);
    //o.target0 = blitterValue;
    
    return o;
}