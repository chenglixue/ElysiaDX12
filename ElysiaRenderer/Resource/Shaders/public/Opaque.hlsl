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
    float4  positionCS   : SV_POSITION;
    float2  uv           : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    float2 screenUV = i.positionCS.xy / screenSize.xy;
    
    //SamplerData samplerData = GetSamplerData();
    
    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);
    
    float3 positionWS = ComputeWorldSpacePosition(screenUV, GBufferData.Depth, viewProjMatrix_I);
    
    FInputParams inputParam = (FInputParams) 0;
    inputParam.PositionWS = positionWS;
    inputParam.PositionVS = mul(float4(positionWS, 1.f), viewMatrix);
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.objectUV = i.uv;
    inputParam.ScreenUV = i.positionCS.xy / screenSize.xy;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    
    LightData mainLightData = GetMainLight(mainLight);
    float shadow = SunShadowVisibility(inputParam.PositionWS, inputParam.ScreenUV, 0);
    
    float4 lighting = GetDynamicLighting(inputParam, GBufferData, mainLightData) * shadow;
    //lighting += float4(GBufferData.IBL, 1.f);
    
    o.target0.rgb = lighting;
    return o;
}