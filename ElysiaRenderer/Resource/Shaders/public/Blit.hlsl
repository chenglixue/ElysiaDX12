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
    
    half4 blitterValue = blitterTex.SampleLevel(linearSampler, i.uv, 0);
    
    o.target0 = blitterValue;
    
    return o;
}