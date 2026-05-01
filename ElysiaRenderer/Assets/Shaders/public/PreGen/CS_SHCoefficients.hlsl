#include "private\Common.hlsl"

#define GROUP_SIZE 8
cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;
    float4 g_SkyboxSize;
    float4 g_SHCoefficientsTempCount;

    UINT g_SHCoefficientsBufferIndex;
    UINT g_SHCoefficientsTempBufferIndex;
}

struct SHCoefficientData
{
    Vector4 SHCoefficients[9];
    float TotalWeight;
    float3 _Padding;
};
groupshared float4 g_SHCoefficients[9][64];
groupshared float g_SHWeights[64];

SHCoefficientData Elysia_Get_Temp_SHCoefficient_Data(UINT index)
{
    RWStructuredBuffer<SHCoefficientData> o = ResourceDescriptorHeap[g_SHCoefficientsTempBufferIndex];
    return o[index];
}
void Elysia_Save_Temp_SHCoefficient_Data(UINT index, SHCoefficientData data)
{
    RWStructuredBuffer<SHCoefficientData> o = ResourceDescriptorHeap[g_SHCoefficientsTempBufferIndex];
    o[index] = data;
}
void Elysia_Save_SHCoefficient_Data(UINT index, float4 saveValue)
{
    RWStructuredBuffer<float4> o = ResourceDescriptorHeap[g_SHCoefficientsBufferIndex];
    o[index] = saveValue;
}
float4 Elysia_Get_Environment(float3 sampleDir)
{
    return SampleTextureCube(SkyboxTexIndex, sampleDir, ClampLinearSampler);
}
float3 GetCubeFaceDirection(float2 uv, int face)
{
    float3 dir = 0;

    switch (face)
    {
    case 0: //+X
        dir.x = 1.0;
        dir.yz = uv.yx * -2.0 + 1.0;
        break;

    case 1: //-X
        dir.x = -1.0;
        dir.y = uv.y * -2.0f + 1.0f;
        dir.z = uv.x * 2.0f - 1.0f;
        break;

    case 2: //+Y
        dir.xz = uv * 2.0f - 1.0f;
        dir.y = 1.0f;
        break;
    case 3: //-Y
        dir.x = uv.x * 2.0f - 1.0f;
        dir.z = uv.y * -2.0f + 1.0f;
        dir.y = -1.0f;
        break;

    case 4: //+Z
        dir.x = uv.x * 2.0f - 1.0f;
        dir.y = uv.y * -2.0f + 1.0f;
        dir.z = 1;
        break;

    case 5: //-Z
        dir.xy = uv * -2.0f + 1.0f;
        dir.z = -1;
        break;
    }
    return normalize(dir);
}
void EvaluateSHBasis(float3 dir, out float Y[9])
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;

    // L0
    Y[0] = 0.282095f;

    // L1
    Y[1] = 0.488603f * y;
    Y[2] = 0.488603f * z;
    Y[3] = 0.488603f * x;

    // L2
    Y[4] = 1.092548f * (x * y);
    Y[5] = 1.092548f * (y * z);
    Y[6] = 0.315392f * (3.0f * z * z - 1.0f);
    Y[7] = 1.092548f * (x * z);
    Y[8] = 0.546274f * (x * x - y * y);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CalcTempSHCoefficients(uint3 globalID : SV_DispatchThreadID,
                            uint3 groupID : SV_GroupID,
                            uint localIdx : SV_GroupIndex)
{
    const UINT2 samplePos = globalID.xy;
    const UINT sampleFace = globalID.z;

    float3 localSH[9] = (float3[9])0;
    float localWeight = 0.0f;
    if (samplePos.x < g_SkyboxSize.x && samplePos.y < g_SkyboxSize.y && sampleFace < 6)
    {
        const float2 sampleUV = ((float2)samplePos + 0.5f) * g_SkyboxSize.zw;
        float3 N = GetCubeFaceDirection(sampleUV, sampleFace);
        float3 radiance = SampleTextureCube(SkyboxTexIndex, N, ClampLinearSampler);
        radiance = pow(radiance, 2.2f);

        float u_coord = (samplePos.x + 0.5f) * g_SkyboxSize.z * 2.0f - 1.0f;
        float v_coord = (samplePos.y + 0.5f) * g_SkyboxSize.w * 2.0f - 1.0f;
        float distSq = 1.0f + u_coord * u_coord + v_coord * v_coord;
        float solidAngle = 4.0f / (sqrt(distSq) * distSq);

        float Y[9];
        EvaluateSHBasis(N, Y);

        [unroll]
        for (UINT i = 0; i < 9; ++i)
        {
            localSH[i] = Y[i] * radiance * solidAngle;
        }
        localWeight = solidAngle;
    }

    [unroll]
    for (UINT i = 0; i < 9; ++i)
    {
        g_SHCoefficients[i][localIdx] = float4(localSH[i], 0.f);
    }
    g_SHWeights[localIdx] = localWeight;

    GroupMemoryBarrierWithGroupSync();

    for (UINT stride = 32; stride >= 1; stride >>= 1)
    {
        if (localIdx < stride)
        {
            for (UINT i = 0; i < 9; ++i)
            {
                g_SHCoefficients[i][localIdx] += g_SHCoefficients[i][localIdx + stride];
            }
            g_SHWeights[localIdx] += g_SHWeights[localIdx + stride];
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (localIdx == 0)
    {
        // float4 totalSH[9] = (float4[9])0;
        // float totalWeight = 0.f;
        //
        // for (int k = 0; k < 64; ++k)
        // {
        //     for (int j = 0; j < 9; ++j)
        //     {
        //         totalSH[j] += g_SHCoefficients[j][k];
        //     }
        //     totalWeight += g_SHWeights[k];
        // }

        const UINT saveIndex = groupID.x +
                               groupID.y * g_SHCoefficientsTempCount.x +
                               groupID.z * g_SHCoefficientsTempCount.x * g_SHCoefficientsTempCount.y;
        SHCoefficientData groupResult;
        for (int i = 0; i < 9; ++i)
        {
            groupResult.SHCoefficients[i] = g_SHCoefficients[i][0];
        }
        groupResult.TotalWeight = g_SHWeights[0];
        groupResult._Padding = float3(0, 0, 0);

        Elysia_Save_Temp_SHCoefficient_Data(saveIndex, groupResult);
    }
}

[numthreads(GROUP_SIZE * GROUP_SIZE, 1, 1)]
void CalcSHCoefficients(uint3 globalID : SV_DispatchThreadID,
                        uint localIdx : SV_GroupIndex)
{
    float4 threadSH[9] = (float4[9])0;
    float threadWeight = 0.0f;

    UINT totalCount = g_SHCoefficientsTempCount.x * g_SHCoefficientsTempCount.y * g_SHCoefficientsTempCount.z;
    for (UINT i = localIdx; i < totalCount; i += GROUP_SIZE * GROUP_SIZE)
    {
        SHCoefficientData data = Elysia_Get_Temp_SHCoefficient_Data(i);
        [unroll]
        for (UINT j = 0; j < 9; ++j)
        {
            threadSH[j] += data.SHCoefficients[j];
        }
        threadWeight += data.TotalWeight;
    }

    [unroll]
    for (int k = 0; k < 9; ++k)
    {
        g_SHCoefficients[k][localIdx] = threadSH[k];
    }
    g_SHWeights[localIdx] = threadWeight;

    GroupMemoryBarrierWithGroupSync();

    for (UINT stride = 32; stride >= 1; stride >>= 1)
    {
        if (localIdx < stride)
        {
            [unroll]
            for (UINT i = 0; i < 9; ++i)
            {
                g_SHCoefficients[i][localIdx] += g_SHCoefficients[i][localIdx + stride];
            }
            g_SHWeights[localIdx] += g_SHWeights[localIdx + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (localIdx == 0)
    {
        float4 finalSH[9] = {(float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0,
                             (float4)0};
        float finalWeight = 0.0f;

        [unroll]
        for (UINT j = 0; j < 9; ++j)
        {
            finalSH[j] += g_SHCoefficients[j][0];
        }
        finalWeight += g_SHWeights[0];
        finalSH[0] *= 0.28209479f;
        finalSH[1] *= 0.32573501f;
        finalSH[2] *= 0.32573501f;
        finalSH[3] *= 0.32573501f;
        finalSH[4] *= 0.27313711f;
        finalSH[5] *= 0.27313711f;
        finalSH[7] *= 0.27313711f;
        finalSH[6] *= 0.07884789f;
        finalSH[8] *= 0.13656855f;

        float normalFactor = FOUR_PI * rcp(finalWeight) + FLT_EPS;
        [unroll]
        for (UINT j = 0; j < 9; ++j)
        {
            finalSH[j] *= normalFactor;
            Elysia_Save_SHCoefficient_Data(j, finalSH[j]);
        }
    }

}