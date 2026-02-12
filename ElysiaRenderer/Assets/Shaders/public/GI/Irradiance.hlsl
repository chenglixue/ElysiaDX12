#ifndef RTXGI_DDGI_IRRADIANCE_HLSL
#define RTXGI_DDGI_IRRADIANCE_HLSL

#include "private\ShadingCommon.hlsl"


// 计算世界空间点对应的网格坐标
float3 GetGridCoord(float3 positionWS,
                    float3 gridOrigin,
                    float3 gridSpacing)
{
    return (positionWS - gridOrigin) / gridSpacing;
}

float3 SampleDDGI(float3 positionWS,
                  float3 normalWS,
                  float3 gridOrigin,
                  float3 gridSpacing,
                  float3 gridDimensions,
                  float4 irradianceTexSize,
                  UINT irradianceTexIndex,
                  UINT samplerIndex)
{
    float3 normalBiasPositionWS = positionWS + normalWS * 0.2f;

    float3 gridCoord = GetGridCoord(normalBiasPositionWS, gridOrigin, gridSpacing);
    int3 baseProbeCoords = floor(gridCoord);
    float3 alpha = frac(gridCoord);

    float3 sumIrradiance = 0.f;
    float sumWeight = 0.f;

    [unroll(8)]
    for (int i = 0; i < 8; ++i)
    {
        // 获取相邻探针的偏移 [0,1]
        int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
        int3 adjCoords = clamp(baseProbeCoords + offset, 0, gridDimensions.xyz - 1);

        // 映射到线性索引
        uint adjProbeIdx = adjCoords.x + adjCoords.y * gridDimensions.x + adjCoords.z * (
                               gridDimensions.x * gridDimensions.y);

        // 4. 计算三线性插值权重
        float3 trilinear = lerp(1.0 - alpha, alpha, (float3)offset);
        float weight = trilinear.x * trilinear.y * trilinear.z;

        // 5. 计算当前探针的采样 UV
        // 使用物体表面的世界法线作为观察方向
        float2 octantCoords = OctEncode(normalWS);

        // 关键：将探针索引映射到你的 8x8 图集中
        // 假设图集布局与 Dispatch 一致：X轴为 Grid.x，Y轴为 Grid.y * Grid.z
        uint2 atlasPos = uint2(adjCoords.x, adjCoords.y + adjCoords.z * gridDimensions.y);

        float2 uv = (octantCoords * 0.5f + 0.5f) * 6.0f + 1.0f;
        float2 finalUV = (float2(atlasPos * 8) + uv) / irradianceTexSize;

        float3 probeColor = SampleTexture2D_LOD(irradianceTexIndex, finalUV, samplerIndex, 0).rgb;

        // NVIDIA 建议在累加前平方，以保持线性感
        sumIrradiance += probeColor * probeColor * weight;
        sumWeight += weight;
    }

    float3 finalIrradiance = (sumWeight > 0) ? (sumIrradiance / sumWeight) : 0;
    return sqrt(finalIrradiance) * 3.14159f;
}

#endif