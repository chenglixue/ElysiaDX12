#ifndef SHADING_COMMON_H
#define SHADING_COMMON_H

#include "SharedCommon.hlsli"
#include "Color.hlsl"

///////////////////////////////////////////////////////////////////////////////
// Shading parameterisation

float F0ToDielectricSpecular(float F0)
{
    return saturate(F0 / 0.08f);
}

float F0RGBToF0(float3 F0)
{
    return dot(0.3333333.xxx, F0);
}

float F0RGBToDielectricSpecular(float3 F0)
{
    return F0ToDielectricSpecular(F0RGBToF0(F0));
}

float DielectricSpecularToF0(float Specular)
{
    return float(0.08f * Specular);
}

// [Burley, "Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering"]
float DielectricF0ToIor(float F0)
{
    return 2.0f / (1.0f - sqrt(min(F0, 0.99))) - 1.0f;
}

float DielectricF0RGBToIor(float3 F0)
{
    return DielectricF0ToIor(F0RGBToF0(F0));
}

float DielectricIorToF0(float Ior)
{
    const float F0Sqrt = (Ior - 1) / (Ior + 1);
    const float F0 = F0Sqrt * F0Sqrt;
    return F0;
}

// Anything with Specular less than 2% is physically impossible and is instead considered to be shadowing.
float GetF0MicroOcclusionThreshold()
{
    return 0.02f;
}
float F0ToMicroOcclusion(float F0)
{
    return saturate(50.0 * F0);
}
float3 F0ToMicroOcclusion(float3 F0)
{
    return saturate(50.0 * F0);
}

float F0RGBToMicroOcclusion(float3 F0)
{
    return F0ToMicroOcclusion(max(F0.r, max(F0.g, F0.b)));
}

float3 ComputeF0(float Specular, float3 BaseColor, float Metallic)
{
    return lerp(DielectricSpecularToF0(Specular).xxx, BaseColor, Metallic.xxx);
}

float3 ComputeF90(float3 F0, float3 EdgeColor, float Metallic)
{
    return lerp(1.0, EdgeColor, Metallic.xxx);
}

float3 ComputeDiffuseAlbedo(float3 BaseColor, float Metallic)
{
    return BaseColor - BaseColor * Metallic;
}

float MakeRoughnessSafe(float Roughness, float MinRoughness = 0.001f)
{
    return clamp(Roughness, MinRoughness, 1.0f);
}

float F0ToMetallic(float F0)
{
    // Approximate the metallic input from F0 with a small lerp region
    const float FullMetalBeginF0 = 0.08f;
    // Instead of DiamondF0 = 0.24, the metallic region starts right after metallic >0 and specular=1 to match with legacy.
    const float FullMetalEndF0 = 0.4f; // roughly the end of semi-conductor
    // This is compatible with UE shading model mapping allowing F0 to take a value up to 0.08 for dielectric.

    return saturate((F0 - FullMetalBeginF0) / (FullMetalEndF0 - FullMetalBeginF0));
}

float F0RGBToMetallic(float3 F0)
{
    return F0ToMetallic(max(F0.r, max(F0.g, F0.b)));
}

float3 EncodeNormal(float3 N)
{
    return N * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 N)
{
    return N * 2.f - 1.f;
    //return OctahedronToUnitVector( Pack888To1212( N ) * 2 - 1 );
}

float EncodeMaterialFlags(uint materialFlags)
{
    return materialFlags * (1.0h / 255.0h);
}

uint DecodeMaterialFlags(float packedMaterialFlags)
{
    return uint((packedMaterialFlags * 255.0h) + 0.5h);
}

//MaterialData GetMaterialData(FInputParams inputParams)
//{
//    MaterialData o = (MaterialData) 0;

//    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);
//    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];

//    Texture2D<float4> baseColorTex = ResourceDescriptorHeap[baseColorTexIndex];
//    float4 baseColor = baseColorTex.Sample(warpLinearSampler, inputParams.objectUV)
//            * float4(baseColorTint, opacity);
//    clip(baseColor.a - cutoff);

//    Texture2D<float4>  normalTex = ResourceDescriptorHeap[normalTexIndex];
//    float4 normalTS = normalTex.Sample(warpLinearSampler, inputParams.objectUV);

//    Texture2D<float>  metallicTex = ResourceDescriptorHeap[metallicTexIndex];
//    float metallic = metallicTex.Sample(warpLinearSampler, inputParams.objectUV);
//    metallic = metallic * metallicIntensity;

//    Texture2D<float>  roughnessTex = ResourceDescriptorHeap[roughnessTexIndex];
//    float roughness = roughnessTex.Sample(warpLinearSampler, inputParams.objectUV);
//    roughness = roughness * roughnessIntensity;

//    o.BaseColor = baseColor.rgb;
//    o.Opacity = baseColor.a;
//    o.AO = 1;
//    o.Metallic = metallic;
//    o.Roughness = roughness;
//    o.Specular = 0.5;

//    o.WorldNormal = g_hasNormalTex ? GetNormal(normalTS.rgb, TBN, normalIntensity) : inputParams.NormalWS;
//    //o.WorldNormal = GetNormal(normalTS.rgb, TBN, normalIntensity, true);

//    o.Anisotropy = 0;
//    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
//    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);

//    return o;
//}

FDecodeGBufferData DecodeGBufferData(float4 InGBuffer0,
                                     float4 InGBuffer1,
                                     float4 InGBuffer2,
                                     float4 InGBuffer3,
                                     float4 InGBuffer4,
                                     float4 InGBufferVelocity,
                                     float4 InGBuffer6,
                                     float SceneDepth)
{
    FDecodeGBufferData o = (FDecodeGBufferData)0;

    o.BaseColor = InGBuffer0.rgb;
    o.ShadingModelID = DecodeMaterialFlags(InGBuffer0.a);

    o.Metallic = InGBuffer1.r;
    o.Specular = InGBuffer1.g;
    o.Roughness = InGBuffer1.b;
    o.AO = InGBuffer1.a;

    o.WorldTangent = DecodeNormal(InGBuffer2.rgb);
    o.Anisotropy = InGBuffer2.a;

    o.WorldNormal = DecodeNormal(InGBuffer3.rgb);
    // o.WorldNormal = normalize(o.WorldNormal);

    o.SceneColor = InGBuffer4.rgb;
    o.Opacity = InGBuffer4.a;

    o.Velocity = InGBufferVelocity.rg;

    o.CustomData = InGBuffer6;

    o.Depth = SceneDepth;

    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);

    return o;
}

FDecodeGBufferData GetDecodeGBufferData(float2 uv)
{
    FDecodeGBufferData o = (FDecodeGBufferData)0;

    float4 GBuffer0 = SampleTexture2D(GBuffer0Index, uv, WarpLinearSampler);
    float4 GBuffer1 = SampleTexture2D(GBuffer1Index, uv, WarpLinearSampler);
    float4 GBuffer2 = SampleTexture2D(GBuffer2Index, uv, WarpLinearSampler);
    float4 GBuffer3 = SampleTexture2D(GBuffer3Index, uv, WarpLinearSampler);
    float4 GBuffer4 = SampleTexture2D(GBuffer4Index, uv, WarpLinearSampler);
    float4 GBuffer5 = SampleTexture2D(GBuffer5Index, uv, WarpLinearSampler);
    float4 GBuffer6 = SampleTexture2D(GBuffer6Index, uv, WarpLinearSampler);
    float sceneDepth = SampleTexture2D(OpaqueDepthIndex, uv, WarpPointSampler).r;

    o = DecodeGBufferData(GBuffer0,
                          GBuffer1,
                          GBuffer2,
                          GBuffer3,
                          GBuffer4,
                          GBuffer5,
                          GBuffer6,
                          sceneDepth);

    return o;
}

float3 SampleNormalWS(float2 uv)
{
    float3 encodeNormalWS = SampleTexture2D(GBuffer3Index, uv, WarpLinearSampler);
    float3 decodeNormalWS = DecodeNormal(encodeNormalWS);
    decodeNormalWS = normalize(decodeNormalWS);

    return decodeNormalWS;
}
float3 SampleNormalWS(float2 uv, UINT samplerIndex)
{
    float3 encodeNormalWS = SampleTexture2D(GBuffer3Index, uv, samplerIndex);
    float3 decodeNormalWS = DecodeNormal(encodeNormalWS);
    decodeNormalWS = normalize(decodeNormalWS);

    return decodeNormalWS;
}
float3 SampleTangentWS(float2 uv, UINT samplerIndex)
{
    float3 encodeNormalWS = SampleTexture2D(GBuffer2Index, uv, samplerIndex);
    float3 decodeNormalWS = DecodeNormal(encodeNormalWS);
    decodeNormalWS = normalize(decodeNormalWS);

    return decodeNormalWS;
}

// Computes world-space position from post-projection depth
//float3 PositionFromDepth(in float zw, in float2 uv)
//{
//    float linearDepth = projMatrix._43 / (zw - projMatrix._33);
//    float4 positionCS = float4(uv * 2.0f - 1.0f, zw, 1.0f);
//    positionCS.y *= -1.0f;
//    float4 positionWS = mul(positionCS, viewProjMatrix_I);
//    return positionWS.xyz / positionWS.w;
//}

float4 ComputeClipSpacePosition(float2 screenUV, float rawDepth)
{
    float4 positionCS = float4(screenUV * 2.0 - 1.0, rawDepth, 1.0);

    positionCS.y = -positionCS.y;

    return positionCS;
}
float4 ComputeClipSpacePosition(float2 ScreenUV, float EyeDepth, Matrix projMatrix)
{
    float TanHalfFovX = rcp(projMatrix[0][0]);
    float TanHalfFovY = rcp(projMatrix[1][1]);

    float2 ScreenNDC = ScreenUV * 2.f - 1.f;
    ScreenNDC.y = -ScreenNDC.y;
    float2 ViewRay = ScreenNDC * float2(TanHalfFovX, TanHalfFovY);

    float3 result = float3(ViewRay * EyeDepth, EyeDepth);

    return float4(result, 1.f);
}

float3 ComputeWorldSpacePosition(float2 screenUV, float rawDepth, Matrix invViewProjMatrix)
{
    float4 positionCS = ComputeClipSpacePosition(screenUV, rawDepth);
    float4 positionWS = mul(positionCS, invViewProjMatrix);
    return positionWS.xyz / positionWS.w;
}

float3 ComputeViewSpacePosition(float2 screenUV, float rawDepth, Matrix projMatrix_I)
{
    float4 positionCS = ComputeClipSpacePosition(screenUV, rawDepth);
    float4 viewPos = mul(positionCS, projMatrix_I);
    return viewPos.xyz / viewPos.w;
}

// Z buffer to linear 0..1 depth (0 at camera position, 1 at far plane).
// Does NOT work with orthographic projections.
// Does NOT correctly handle oblique view frustums.
// zBufferParam (UNITY_REVERSED_Z) = { f/n - 1,   1, (1/n - 1/f), 1/f }
// zBufferParam                    = { 1 - f/n, f/n, (1/f - 1/n), 1/n }
float Linear01Depth(float depth, float4 zBufferParam)
{
    return 1.0 / (zBufferParam.x * depth + zBufferParam.y);
}

// Z buffer to linear view space (eye) depth.
// Does NOT correctly handle oblique view frustums.
// Does NOT work with orthographic projection.
// zBufferParam (UNITY_REVERSED_Z) = { f/n - 1,   1, (1/n - 1/f), 1/f }
// zBufferParam                    = { 1 - f/n, f/n, (1/f - 1/n), 1/n }
float LinearEyeDepth(float depth, float4 zBufferParam)
{
    return 1.0 / (zBufferParam.z * depth + zBufferParam.w);
}

float ComputeTemporalDither(float2 screenPos, uint frameIndex)
{
    static const float ditherMatrix[16] = {
        0.0f / 16.0f, 8.0f / 16.0f, 2.0f / 16.0f, 10.0f / 16.0f,
        12.0f / 16.0f, 4.0f / 16.0f, 14.0f / 16.0f, 6.0f / 16.0f,
        3.0f / 16.0f, 11.0f / 16.0f, 1.0f / 16.0f, 9.0f / 16.0f,
        15.0f / 16.0f, 7.0f / 16.0f, 13.0f / 16.0f, 5.0f / 16.0f
    };

    uint x = (uint)screenPos.x % 4;
    uint y = (uint)screenPos.y % 4;

    uint temporalOffset = frameIndex % 16;

    uint index = (x + y * 4 + temporalOffset) % 16;

    return ditherMatrix[index];
}
#endif