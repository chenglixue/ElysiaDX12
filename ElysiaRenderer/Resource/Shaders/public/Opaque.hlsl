#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\ShadowCommon.hlsl>

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Disabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 screenSize;
    
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
    
    UINT g_AOIndex;

}

struct PSInput
{
    float4  positionCS   : SV_POSITION;
    float2  uv           : TEXCOORD0;
};

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
    
    float2 screenUV = i.positionCS.xy / screenSize.xy;
    
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
    float shadow = SunShadowVisibility(inputParam.PositionWS, inputParam.ScreenUV, shadowSize, shadowMatrix);
    
    Texture2D<float> AOTex = ResourceDescriptorHeap[g_AOIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    float AO = AOTex.Sample(warpLinearSampler, inputParam.ScreenUV);
    AO = 1;
    
    float4 lighting = GetDynamicLighting(inputParam, GBufferData, mainLightData, AO);
    lighting += float4(GBufferData.SceneColor, 1.f) * AO;
    
    o.target0.rgb = lighting;
    return o;
}