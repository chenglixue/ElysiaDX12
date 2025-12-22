#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\ShadowCommon.hlsl>

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer BackFaceCull
#pragma Blend Disabled
#pragma Depth WritesEnabled

cbuffer ObjectConstant : register(b0, perObjectSpace)
{
    Matrix worldMatrix;
};

cbuffer MaterialConstant : register(b0, perMaterialSpace)
{   
    float opacity;
    float cutoff;
    UINT baseColorTexIndex;
    UINT normalTexIndex;
    
    UINT metallicTexIndex;
    UINT roughnessTexIndex;
    UINT specularTexIndex;
    
    Vector3 baseColorTint;
    
    Vector3 ambientCubemapTint;
    float normalIntensity;
    
    float metallicIntensity;
    float roughnessIntensity;
    float ambientCubemapIntensity;
    float g_hasNormalTex;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 screenSize;
    
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
}

struct VSInput
{
    float3 positionOS : POSITION;
    float2 uv : TEXCOORD0;
    float3 normalOS : NORMAL;
    float3 tangentOS : TANGENT;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float4 positionVS : VIEW_POSITION;
    float4 positionWS : WORLD_POSITION;
    float3 normalWS : NORMALWS;
    float3 tangentWS : TANGENTWS;
    float3 bitTangentWS : BITANGENTWS;
    float2 uv : TEXCOORD0;
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

    o.positionWS = mul(float4(i.positionOS, 1.f), worldMatrix);
    o.positionVS = mul(o.positionWS, viewMatrix);
    o.positionCS = mul(o.positionVS, projMatrix);
    
    float3 N = normalize(mul(i.normalOS, (float3x3) worldMatrix));
    float3 T = normalize(mul(i.tangentOS, (float3x3) worldMatrix));
    o.tangentWS = normalize(T - dot(N, T) * N);
    o.bitTangentWS = (cross(o.tangentWS, N));
    o.normalWS = N;
    
    //float handedness = dot(o.bitTangentWS, cross(o.normalWS, o.tangentWS)) > 0.0f ? 1.0f : -1.0f;
    //o.bitTangentWS *= handedness;
    
    o.uv = i.uv;
    
    return o;
}

FEncodeGBufferData GetEncodeGBufferData(FInputParams inputParams, float3 toLight);

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
    
    FEncodeGBufferData encodeGBufferData = GetEncodeGBufferData(inputParam, mainLightData.toLight);
    
    o.target0 = float4(encodeGBufferData.BaseColor, EncodeMaterialFlags(encodeGBufferData.ShadingModelID));
    o.target1 = float4(encodeGBufferData.Metallic, encodeGBufferData.Specular, encodeGBufferData.Roughness, encodeGBufferData.AO);
    o.target2 = float4(EncodeNormal(encodeGBufferData.WorldTangent), encodeGBufferData.Anisotropy);
    o.target3 = float4(EncodeNormal(encodeGBufferData.WorldNormal), encodeGBufferData.PerObjectData);
    o.target4 = float4(encodeGBufferData.IBL * encodeGBufferData.AO, encodeGBufferData.Opacity);
    o.target5 = float4(encodeGBufferData.Velocity, 0.f, 0.f);

    return o;
}

FEncodeGBufferData GetEncodeGBufferData(FInputParams inputParams, float3 toLight)
{
    FEncodeGBufferData o = (FEncodeGBufferData) 0;
    
    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
    float4 baseColor = baseColorTex.Sample(warpLinearSampler, inputParams.objectUV)
            * float4(baseColorTint, opacity);
    baseColor.rgb = AMDTonemapInvert(baseColor);
    clip(baseColor.a - cutoff);

    Texture2D<float4> normalTex = ResourceDescriptorHeap[normalTexIndex];
    float4 normalTS = normalTex.Sample(warpLinearSampler, inputParams.objectUV);

    Texture2D<float> metallicTex = ResourceDescriptorHeap[metallicTexIndex];
    float metallic = metallicTex.Sample(warpLinearSampler, inputParams.objectUV);
    metallic = metallic * metallicIntensity;
    
    Texture2D<float> roughnessTex = ResourceDescriptorHeap[roughnessTexIndex];
    float roughness = roughnessTex.Sample(warpLinearSampler, inputParams.objectUV);
    roughness = roughness * roughnessIntensity;
    
    o.BaseColor = baseColor.rgb;
    o.ShadingModelID = FLT_MAX;
    o.ShadingModelID = Shading_Model_ID_Default_Lit;
    o.Opacity = baseColor.a;
    
    o.AO = 1;
    o.Metallic = metallic;
    o.Roughness = roughness;
    o.Specular = 0.5;
    
    o.WorldNormal = g_hasNormalTex ? GetNormal(normalTS.rgb, TBN, normalIntensity) : inputParams.NormalWS;
    o.WorldTangent = TBN._m00_m01_m02;
    o.PerObjectData = 0.f;
    o.PerComputedShadow = 1.f;
    
    o.Velocity = 0.f;
    
    o.Anisotropy = 0;
    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);
    o.IBL = GetIBL(inputParams, o, toLight, ambientCubemapIntensity, ambientCubemapTint);
 
    return o;
}