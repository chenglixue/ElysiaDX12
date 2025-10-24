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
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
    float4 target1 : SV_TARGET1;
    float4 target2 : SV_TARGET2;
    float4 target3 : SV_TARGET3;
    float4 target4 : SV_TARGET4;
    float4 target5 : SV_TARGET5;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput) 0;

    o.positionWS = mul(worldMatrix, float4(i.positionOS, 1.f));
    o.positionVS = mul(viewMatrix, o.positionWS);
    o.positionCS = mul(projMatrix, o.positionVS);
    
    float3 N = normalize(mul((float3x3) worldMatrix, i.normalOS));
    float3 T = mul((float3x3) worldMatrix, i.tangentOS);
    o.tangentWS = normalize(T - dot(N, T) * N);
    o.bitTangentWS = (cross(o.tangentWS, N));
    o.normalWS = N;
    
    o.uv = i.uv;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    FInputParams inputParam = (FInputParams) 0;
    inputParam.PositionWS = i.positionWS;
    inputParam.PositionVS = i.positionVS;
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.objectUV = i.uv;
    inputParam.ScreenUV = i.positionCS.xy / screenSize.xy;
    inputParam.TangentWS = i.tangentWS;
    inputParam.BitTangentWS = i.bitTangentWS;
    inputParam.NormalWS = i.normalWS;
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, i.positionWS.xyz);
    
    LightData mainLightData = GetMainLight(mainLight);
    MaterialData materialData = GetMaterialData(inputParam);
    
    FDecodeGBufferData decodeGBufferData = GetDecodeGBufferData(inputParam, mainLightData.toLight);
    
    o.target0 = float4(decodeGBufferData.BaseColor, EncodeMaterialFlags(decodeGBufferData.ShadingModelID));
    o.target1 = float4(decodeGBufferData.Metallic, decodeGBufferData.Specular, decodeGBufferData.Roughness, decodeGBufferData.AO);
    o.target2 = float4(EncodeNormal(decodeGBufferData.WorldTangent), decodeGBufferData.Anisotropy);
    o.target3 = float4(EncodeNormal(decodeGBufferData.WorldNormal), decodeGBufferData.PerObjectData);
    o.target4 = float4(decodeGBufferData.IBL * decodeGBufferData.AO, decodeGBufferData.Opacity);
    o.target5 = float4(decodeGBufferData.Velocity, 0.f, 0.f);

    
    return o;
}