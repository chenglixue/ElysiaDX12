#include <private\SharedCommon.hlsli>
#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Reversed

#pragma shader_feature SHADOW_QUALITY_LOW SHADOW_QUALITY_MIDDLE SHADOW_QUALITY_HIGH SHADOW_QUALITY_VERYHIGH
#pragma shader_feature HARD_SHADOW SOFT_SHADOW

#define DEBUG_NONE 0
#define DEBUG_AO 1
#define DEBUG_GIPROBE 2
#define DEBUG_NORMAL 3
#define DEBUG_AABB 4
#define DEBUG_BLOOM 5
#define DEBUG_VELOCITY 6
#define DEBUG_GI 7

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_RenderSize;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float3 g_GridDimensions;
    float g_ProbeNormalBias;
    float g_ProbeViewBias;
    float g_DDGIEncodingGamma;
    float3 g_GridOrigin;
    float3 g_GridSpacing;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetsIndex;
    UINT g_ProbeStatesIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_ProbeRelocationLUTBufferIndex;
    float4 g_IrradianceTexSize;
    float4 g_DistanceTexSize;
    float3 g_AmbientTint;
    float g_AmbientIntensity;
    UINT g_DebugMode;

    UINT g_ShadowMaskTexIndex;
    UINT g_PreIntegrateSSSLUTIndex;
    UINT g_PreIntegrateSSSNDFLUTIndex;
    float g_CurveScale;
    float g_ScatterRadius;
    float g_TransmissionScale;
    float g_TransmissionRange;
    float g_TransmissionEdgeGlow;

    UINT g_SHCoefficientsBufferIndex;
}

#include <private\AmbientCubemap.hlsl>
#include <private\ShadingCommon.hlsl>
#include <private\ShadingModel.hlsl>

#include <private\LightCommon.hlsl>
#include <private\Light.hlsl>
#include <public\GI\Irradiance.hlsl>

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput)0;

    o.uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.positionCS = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 1.0f, 1.0f);

    // if (vertexID == 0)
    // {
    //     o.positionCS = float4(-1.0f, 1.0f, 1.0f, 1.0f);
    //     o.uv = float2(0.0f, 0.0f);
    // }
    // else if (vertexID == 1)
    // {
    //     o.positionCS = float4(3.0f, 1.0f, 0.0f, 1.0f);
    //     o.uv = float2(2.0f, 0.0f);
    // }
    // else
    // {
    //     o.positionCS = float4(-1.0f, -3.0f, 0.0f, 1.0f);
    //     o.uv = float2(0.0f, 2.0f);
    // }

    return o;
}


PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    float2 screenUV = i.positionCS.xy * g_RenderSize.zw;

    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);

    float3 positionWS = ComputeWorldSpacePosition(screenUV, GBufferData.Depth, viewProjMatrix_I);

    FInputParams inputParam = (FInputParams)0;
    inputParam.PositionWS = positionWS;
    inputParam.PositionVS = mul(float4(positionWS, 1.f), viewMatrix);
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.objectUV = i.uv;
    inputParam.ScreenUV = i.positionCS.xy / g_RenderSize.xy;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    inputParam.ScreenSize = g_RenderSize.xy;

    LightData mainLightData = GetMainLight(mainLight);

    float AO = SampleTexture2D(g_AOIndex, inputParam.ScreenUV, WarpPointSampler);
    if (!g_EnableAO)
    {
        AO = 1;
    }
    float4 lighting = GetDynamicLighting(inputParam, GBufferData, mainLightData, AO);
    float blendWeight = DDGIGetVolumeBlendWeight(inputParam.PositionWS,
                                                 g_GridOrigin,
                                                 g_GridSpacing,
                                                 0,
                                                 float4(0, 0, 0, 1));
    float3 IBL = 0.f;
    if (blendWeight > 0)
    {
        float3 IBL = SampleDDGI(inputParam.PositionWS,
                                inputParam.NormalWS,
                                DDGIGetSurfaceBias(inputParam.NormalWS,
                                                   inputParam.ScreenVector,
                                                   g_ProbeNormalBias,
                                                   g_ProbeViewBias),
                                g_GridOrigin,
                                g_GridSpacing,
                                g_GridDimensions,
                                g_DDGIEncodingGamma,
                                g_IrradianceTexSize,
                                g_IrradianceTexIndex,
                                g_DistanceTexSize,
                                g_DistanceTexIndex,
                                g_ProbeOffsetsIndex,
                                g_ProbeStatesIndex,
                                WarpLinearSampler
                         ) * g_AmbientTint * g_AmbientIntensity * blendWeight;
        IBL *= (GBufferData.DiffuseColor.rgb) / PI;
        lighting += float4(IBL, 1.f) * AO;
    }
    IBL += GetIBL(inputParam, GBufferData, mainLightData.toLight, g_AmbientIntensity, g_AmbientTint);
    lighting.rgb += (GBufferData.SceneColor + IBL) * AO;

    switch (g_DebugMode)
    {
    case DEBUG_GI:
        o.target0.rgb = (IBL);
        return o;
        break;
    }
    o.target0 = lighting;
    return o;
}