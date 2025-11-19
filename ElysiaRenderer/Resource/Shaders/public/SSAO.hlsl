#if EDITOR
#include <public\Blit.hlsl>
#include <private\ShadingCommon.hlsl>
#include <private\SharedCommon.hlsli>
#else
#include "Blit.hlsl"
#include "../private\ShadingCommon.hlsl"
#include "../private\SharedCommon.hlsli"
#endif

#define _AO_MAX_SAMPLE_COUNT 256

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_ScreenSize;
    
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
    
    UINT g_AOSampleCount;
    float g_AORadius;
    float g_AOThreshold;
    float g_AODepthBias;
    
    float4 g_AOSampleKernelArray[_AO_MAX_SAMPLE_COUNT];

}

struct PSOutput
{
    float target0 : SV_TARGET0;
};

float3 GetRandomVec(float2 p);
float3 GetRandomVecHalf(float2 p);

PSOutput SSAOPS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    float2 screenUV = i.positionCS.xy / g_ScreenSize.xy;
    
    FDecodeGBufferData GBufferData = GetDecodeGBufferData(screenUV);
    
    float3 positionWS = ComputeWorldSpacePosition(screenUV, GBufferData.Depth, viewProjMatrix_I);
    
    FInputParams inputParam = (FInputParams) 0;
    inputParam.PositionWS = positionWS;
    inputParam.PositionVS = mul(float4(positionWS, 1.f), viewMatrix);
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.objectUV = i.uv;
    inputParam.ScreenUV = screenUV;
    inputParam.TangentWS = GBufferData.WorldTangent;
    inputParam.NormalWS = GBufferData.WorldNormal;
    inputParam.BitTangentWS = cross(inputParam.TangentWS, inputParam.NormalWS);
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, positionWS);
    inputParam.Linear01Depth = Linear01Depth(GBufferData.Depth, g_ZBufferParams);
    inputParam.LinearEyeDepth = LinearEyeDepth(GBufferData.Depth, g_ZBufferParams);
    
    float2 noiseScale = g_ScreenSize.xy * rcp(4.f);
    float2 noiseUV = i.uv * noiseScale;
    
    float AO = 0.f;
    float3 tangentWS = GetRandomVec(inputParam.ScreenUV);
    float3 bitangentWS = normalize(cross(tangentWS, GBufferData.WorldNormal));
    float3x3 TBN = float3x3(tangentWS, bitangentWS, GBufferData.WorldNormal);
    
    [unroll(256)]
    for (UINT i = 0; i < g_AOSampleCount; ++i)
    {
        // 法线半球的随机向量
        float3 randomVec = mul(g_AOSampleKernelArray[i].xyz, TBN);
        randomVec = GetRandomVecHalf(i * inputParam.ScreenUV);
        float scale = i / g_AOSampleCount;
        scale = lerp(0.01f, 1.f, Pow2(scale));
        randomVec *= g_AORadius * scale;
        float AOWeight = smoothstep(0.f, 0.2f, length(randomVec));
        randomVec = mul(randomVec, TBN);
        
        float4 randomPosWS = float4(randomVec, 0.f) + positionWS;
        float4 randomPosVS = mul(viewMatrix, randomPosWS);
        float4 randomPosCS = mul(projMatrix, randomPosVS);
        float2 randomPosUV = randomPosCS.xy / randomPosCS.w * 0.5f + 0.5f;
        
        Texture2D<float> OpaqueDepth = ResourceDescriptorHeap[OpaqueDepthIndex];
        float randomDepth = OpaqueDepth.SampleLevel(warpLinearSampler, randomPosUV, 0);
        float randomLinear01Depth = Linear01Depth(randomDepth, g_ZBufferParams);
        float randomEyeDepth = LinearEyeDepth(randomDepth, g_ZBufferParams);
        float randomZ = randomPosCS.w;
        
        //float range = step(InputParams.Linear01Depth, randomLinear01Depth + _AODepthBias);
        float range = step(g_AOThreshold, (randomLinear01Depth + g_AODepthBias - inputParam.Linear01Depth));
        range = step(randomEyeDepth, randomZ);
        float rangeCheck = smoothstep(0.f, 1.f, g_AORadius / abs(randomZ - randomEyeDepth));
        
        AO += range * AOWeight * rangeCheck;
    }
    
    return o;
}

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float3 GetRandomVec(float2 p)
{
    float3 vec = float3(0, 0, 0);
    vec.x = Hash(p) * 2 - 1;
    vec.y = Hash(p * p) * 2 - 1;
    vec.z = Hash(p * p * p) * 2 - 1;
    return normalize(vec);
}

float3 GetRandomVecHalf(float2 p)
{
    float3 vec = float3(0, 0, 0);
    vec.x = Hash(p) * 2 - 1;
    vec.y = Hash(p * p) * 2 - 1;
    vec.z = saturate(Hash(p * p * p) + 0.2);
    return normalize(vec);
}