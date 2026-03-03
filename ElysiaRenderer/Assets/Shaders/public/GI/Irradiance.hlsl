#ifndef RTXGI_DDGI_IRRADIANCE_HLSL
#define RTXGI_DDGI_IRRADIANCE_HLSL

#include "private\ShadingCommon.hlsl"

#define MIN_DISTANCE_BETWEEN_PROBES 1.f

// 计算世界空间点对应的网格坐标
float3 GetGridCoord(float3 positionWS,
                    float3 gridOrigin,
                    float3 gridSpacing)
{
    return (positionWS - gridOrigin) / gridSpacing;
}

/**
 * Computes the surfaceBias parameter used by DDGIGetVolumeIrradiance().
 * The surfaceNormal and cameraDirection arguments are expected to be normalized.
 */
float3 DDGIGetSurfaceBias(float3 surfaceNormal,
                          float3 cameraDirection,
                          float3 probeNormalBias,
                          float3 probeViewBias)
{
    float3 o = (surfaceNormal * probeNormalBias) + (-cameraDirection * probeViewBias);
    // o *= 0.3f * (0.75f * MIN_DISTANCE_BETWEEN_PROBES);
    return o;
}

// GBuffer调用
float3 SampleDDGI(float3 positionWS,
                  float3 normalWS,
                  float3 surfaceBias,
                  float3 gridOrigin,
                  float3 gridSpacing,
                  float3 gridDimensions,
                  float gamma,
                  float4 irradianceTexSize,
                  UINT irradianceTexIndex,
                  float4 distanceTexSize,
                  UINT distanceTexIndex,
                  UINT probeOffsetIndexTexIndex,
                  UINT ProbeRelocationLUTBufferIndex,
                  UINT ProbeStatesIndex,
                  UINT samplerIndex)
{
    float3 biasPositionWS = positionWS + surfaceBias;

    float3 gridCoord = GetGridCoord(biasPositionWS, gridOrigin, gridSpacing);
    int3 baseProbeCoords = floor(gridCoord);
    float3 alpha = frac(gridCoord);

    float3 sumIrradiance = 0.f;
    float sumWeight = 0.f;
    // StructuredBuffer<float3> probeOffsets = ResourceDescriptorHeap[ProbeOffsetsIndex];
    StructuredBuffer<UINT> probeStates = ResourceDescriptorHeap[ProbeStatesIndex];

    [unroll(8)]
    for (int i = 0; i < 8; ++i)
    {
        // 获取相邻探针的偏移 [0,1]
        int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
        int3 adjCoords = clamp(baseProbeCoords + offset, 0, gridDimensions.xyz - 1);

        // 映射到线性索引
        uint adjProbeIdx = adjCoords.x + adjCoords.y * gridDimensions.x + adjCoords.z * (
                               gridDimensions.x * gridDimensions.y);

        UINT2 probeOffsetIndexID = UINT2(adjProbeIdx % 64, adjProbeIdx / 64);
        RWTexture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[probeOffsetIndexTexIndex];
        UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
        StructuredBuffer<Vector4> ProbeRelocationLUTBuffer = ResourceDescriptorHeap[ProbeRelocationLUTBufferIndex];
        float3 probeOffset = ProbeRelocationLUTBuffer[index];

        UINT probeState = probeStates[adjProbeIdx];
        if (probeState == PROBE_STATE_INACTIVE)
            continue;

        // float3 posOffset = probeOffsets[adjProbeIdx];
        float3 posOffset = probeOffset;
        // 获取相邻探针的世界坐标
        float3 adjProbeWorldPos = gridOrigin + adjCoords * gridSpacing + posOffset;

        // 点到探针的方向与距离
        float3 positionWSToAdjProbe = normalize(adjProbeWorldPos - positionWS);
        float3 biasPositionWSToAdjProbe = normalize(adjProbeWorldPos - biasPositionWS);
        float biasPositionWSToAdjProbeDist = length(adjProbeWorldPos - biasPositionWS);

        // 三线性插值权重
        float3 trilinear = lerp(1.0 - alpha, alpha, (float3)offset);
        float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;
        float weight = 1.f;

        // 方向性权重 (Wrap Shading)
        // NVIDIA 方案：让侧面探针有贡献，完全背面的权重降为 0
        // 平方使权重分布更“陡峭”，减少来自背后的渗漏
        float wrapShading = (dot(positionWSToAdjProbe, normalWS) + 1.f) * 0.5f;
        weight *= (wrapShading * wrapShading) + 0.2f; // 留一点底色，防止在极端转角处全黑

        // 计算当前探针的采样 UV
        // 使用物体表面的世界法线作为观察方向
        float2 octantCoords = OctEncode(-biasPositionWSToAdjProbe);
        // 关键：将探针索引映射到图集中
        // 假设图集布局与 Dispatch 一致：X轴为 Grid.x，Y轴为 Grid.y * Grid.z
        uint2 atlasPos = uint2(adjCoords.x, adjCoords.y + adjCoords.z * gridDimensions.y);
        float2 uv = (octantCoords * 0.5f + 0.5f) * (DDGI_PROBE_NUM_TEXELS - 2.f) + 1.0f;
        float2 finalUV = (float2(atlasPos * DDGI_PROBE_NUM_TEXELS) + uv) * distanceTexSize.zw;

        // 采样平均距离 (R) 和距离平方 (G)
        float2 moments = SampleTexture2D_LOD(distanceTexIndex, finalUV, samplerIndex, 0).rg;
        float mean = moments.x;
        float mean2 = moments.y;
        float chebyshevWeight = 1.0f;
        if (biasPositionWSToAdjProbeDist > mean) // 如果点到探针的距离超过了探针记录的平均遮挡深度
        {
            // 防止浮点精度误差导致负数
            float variance = abs(Pow2(mean) - mean2);

            // Chebyshev Visibility
            chebyshevWeight = variance / (variance + Pow2(biasPositionWSToAdjProbeDist - mean));

            // 增强对比度，使遮挡边缘更锐利，减少颜色渗漏
            chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0f);
        }
        // 避免权重完全为 0 导致全黑，设定一个极小的底值
        weight *= max(0.05f, chebyshevWeight);
        weight = max(1e-6f, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
        {
            weight *= (weight * weight) * (1.f / (crushThreshold * crushThreshold));
        }
        weight *= trilinearWeight;

        float2 octantCoordsIrr = OctEncode(normalWS);
        uv = (octantCoordsIrr * 0.5f + 0.5f) * (DDGI_PROBE_NUM_TEXELS - 2.f) + 1.0f;
        finalUV = (float2(atlasPos * DDGI_PROBE_NUM_TEXELS) + uv) * irradianceTexSize.zw;
        float3 probeColor = SampleTexture2D(irradianceTexIndex, finalUV, samplerIndex).rgb;

        float3 exponent = gamma * 0.5f;
        probeColor = pow(probeColor, exponent);

        // NVIDIA 建议在累加前平方，以保持线性感
        sumIrradiance += probeColor * weight;
        sumWeight += weight;
    }

    if (sumWeight <= 0.f)
        return 0.f;

    float3 finalIrradiance = sumIrradiance / sumWeight;
    finalIrradiance *= finalIrradiance;
    finalIrradiance *= TWO_PI;
    return finalIrradiance;
}

// closesthit调用
float3 SampleDDGI(float3 positionWS,
                  float3 normalWS,
                  float3 surfaceBias,
                  float3 gridOrigin,
                  float3 gridSpacing,
                  float3 gridDimensions,
                  float gamma,
                  float4 irradianceTexSize,
                  UINT irradianceTexIndex,
                  float4 distanceTexSize,
                  UINT distanceTexIndex,
                  UINT probeOffsetIndexTexIndex,
                  // StructuredBuffer<float3> ProbeOffsets,
                  StructuredBuffer<Vector4> ProbeRelocationLUTBuffer,
                  StructuredBuffer<UINT> ProbeStates,
                  SamplerState linearClampSampler)
{
    float3 biasPositionWS = positionWS + surfaceBias;

    float3 gridCoord = GetGridCoord(biasPositionWS, gridOrigin, gridSpacing);
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
        UINT2 probeOffsetIndexID = UINT2(adjProbeIdx % 64, adjProbeIdx / 64);
        RWTexture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[probeOffsetIndexTexIndex];
        UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
        float3 probeOffset = ProbeRelocationLUTBuffer[index];
        UINT probeState = ProbeStates[adjProbeIdx];
        if (probeState == PROBE_STATE_INACTIVE)
            continue;

        // float3 posOffset = ProbeOffsets[adjProbeIdx];
        float3 posOffset = probeOffset;
        // 获取相邻探针的世界坐标
        float3 adjProbeWorldPos = gridOrigin + adjCoords * gridSpacing + posOffset;

        // 点到探针的方向与距离
        float3 positionWSToAdjProbe = normalize(adjProbeWorldPos - positionWS);
        float3 biasPositionWSToAdjProbe = normalize(adjProbeWorldPos - biasPositionWS);
        float biasPositionWSToAdjProbeDist = length(adjProbeWorldPos - biasPositionWS);

        // 三线性插值权重
        float3 trilinear = max(0.001f, lerp(1.0 - alpha, alpha, (float3)offset));
        float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;
        float weight = 1.f;

        // 方向性权重 (Wrap Shading)
        // 侧面探针有贡献，完全背面的权重降为 0
        // 平方使权重分布更“陡峭”，减少来自背后的渗漏
        float wrapShading = (dot(positionWSToAdjProbe, normalWS) + 1.f) * 0.5f;
        weight *= (wrapShading * wrapShading) + 0.2f; // 留一点底色，防止在极端转角处全黑

        // 计算当前探针的采样 UV
        // 使用物体表面的世界法线作为观察方向
        float2 octantCoords = OctEncode(-biasPositionWSToAdjProbe);
        // 关键：将探针索引映射到图集中
        // 假设图集布局与 Dispatch 一致：X轴为 Grid.x，Y轴为 Grid.y * Grid.z
        uint2 atlasPos = uint2(adjCoords.x, adjCoords.y + adjCoords.z * gridDimensions.y);
        float2 uv = (octantCoords * 0.5f + 0.5f) * (DDGI_PROBE_NUM_TEXELS - 2.f) + 1.0f;
        float2 finalUV = (float2(atlasPos * DDGI_PROBE_NUM_TEXELS) + uv) * distanceTexSize.zw;

        // 采样平均距离 (R) 和距离平方 (G)
        float2 moments = SampleTexture2D_LOD(distanceTexIndex, finalUV, linearClampSampler, 0).rg;
        float mean = moments.x;
        float mean2 = moments.y;
        float chebyshevWeight = 1.0f;
        if (biasPositionWSToAdjProbeDist > mean) // 如果点到探针的距离超过了探针记录的平均遮挡深度
        {
            // 防止浮点精度误差导致负数
            float variance = abs(Pow2(mean) - mean2);

            // Chebyshev Visibility
            chebyshevWeight = variance / (variance + Pow2(biasPositionWSToAdjProbeDist - mean));

            // 增强对比度，使遮挡边缘更锐利，减少颜色渗漏
            chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0f);
        }
        // 避免权重完全为 0 导致全黑，设定一个极小的底值
        weight *= max(0.05f, chebyshevWeight);
        weight = max(1e-6f, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
        {
            weight *= (weight * weight) * (1.f / (crushThreshold * crushThreshold));
        }
        weight *= trilinearWeight;

        float2 octantCoordsIrr = OctEncode(normalWS);
        uv = (octantCoordsIrr * 0.5f + 0.5f) * (DDGI_PROBE_NUM_TEXELS - 2.f) + 1.0f;
        finalUV = (float2(atlasPos * DDGI_PROBE_NUM_TEXELS) + uv) * irradianceTexSize.zw;
        float3 probeColor = SampleTexture2D_LOD(irradianceTexIndex, finalUV, linearClampSampler, 0).rgb;
        probeColor = pow(probeColor, gamma * 0.5f);

        sumIrradiance += probeColor * weight;
        sumWeight += weight;
    }

    if (sumWeight <= 0.f)
        return 0.f;

    float3 finalIrradiance = sumIrradiance / sumWeight;
    finalIrradiance *= finalIrradiance; // 还原回线性空间
    finalIrradiance *= TWO_PI;
    return finalIrradiance;
}

#endif