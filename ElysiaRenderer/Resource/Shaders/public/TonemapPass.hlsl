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

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    Texture2D blitterTex = ResourceDescriptorHeap[blitterTextureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[ClampLinearSampler];
    
    float4 blitterValue = blitterTex.SampleLevel(linearSampler, i.uv, 0);
    
    float4 linearColor = GetSRGBToLinear(blitterValue);
    
    float3 tonemapColor = NeutralTonemap(linearColor.rgb);
    tonemapColor = GetLinearToSRGB(tonemapColor);
    
    o.target0 = float4(tonemapColor, 1.f);
    
    return o;
}