#if EDITOR
#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\ShadowCommon.hlsl>
#else
#include "../private\ShadingCommon.hlsl"
#include "../private\Light.hlsl"
#include "../private\LightCommon.hlsl"
#include "../private\ShadowCommon.hlsl"
#endif

cbuffer ObjectConstant : register(b0, perObjectSpace)
{
    Matrix worldMatrix;
};

cbuffer MaterialConstant : register(b0, perMaterialSpace)
{
    UINT baseColorTexIndex;
    
    float opacity;
    float cutoff;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    float shadowNearZ;
    float shadowFarZ;
    float shadowDepthBias;
    float shadowSlopeDepthBias;
    float shadowMaxSlopeDepthBias;
    
    Vector2 g_sobolSequence[64];
};

#define DepthState DepthWritesEnabled
#define BlendState BlendDisable
#define RasterizerState RasterizerBackFaceCull

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
    
    o.normalWS = normalize(mul(i.normalOS, (float3x3) worldMatrix));

    o.positionWS = mul(float4(i.positionOS, 1.f), worldMatrix);
    o.positionCS = mul(o.positionWS, shadowMatrix);
    
    LightData mainLightData = GetMainLight(mainLight);
    
    float3 lightDirWS = mainLightData.toLight;
    const float NoL = dot(o.normalWS, lightDirWS);
    
    o.positionWS.rgb += GetShadowDepthOffset(NoL, o.positionCS, shadowSize.x, shadowDepthBias, shadowSlopeDepthBias, shadowMaxSlopeDepthBias);
    
    o.uv = i.uv;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[GBuffer4Index];
        
    float4 baseColor = baseColorTex.Sample(warpLinearSampler, i.uv) * opacity;
    clip(baseColor.a - cutoff);
    
    return o;
}