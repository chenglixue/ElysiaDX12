#ifndef DDGI_COMMON_H
#define DDGI_COMMON_H
#include "ShadingCommon.hlsl"

#define PROBE_COUNT 1024
#define Rays_Per_Probe 32

struct Vertex
{
    float3 positionOS;
    float2 uv;
    float3 normalOS;
    float3 tangentOS;
};

struct RayData
{
    float3 Radiance;
    float Distance;
};

struct InstanceData
{
    UINT BaseColorTexIndex;
    UINT NormalTexIndex;
    UINT MetallicTexIndex;
    UINT RoughnessTexIndex;

    UINT VertexOffset;
    UINT IndexOffset;
    UINT VertexBufferIndex;
    UINT IndexBufferIndex;
};

float2 SignNotZero(float2 v)
{
    return float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// 3D dir normalize to [-1, 1]
float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 result = n.xy;
    if (n.z < 0.0)
    {
        result = (1.0 - abs(result.yx)) * SignNotZero(result);
    }
    return result;
}

// [-1, 1] to normalized 3D dir
float3 OctDecode(float2 f)
{
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += (n.xy >= 0.0) ? -t : t;
    return normalize(n);
}

float2 GetProbeUV(uint probeIndex, float2 octUV, float3 gridDims, float probeRes)
{
    // 1. 将 [-1, 1] 映射到 [0, 1]
    float2 normalizedOctUV = octUV * 0.5 + 0.5;

    // 2. 找到探针在 Atlas 中的 2D 索引 
    uint probeX = probeIndex % (uint)gridDims.x;
    uint probeY = probeIndex / (uint)gridDims.x;

    // 3. 计算该探针 8x8 块的左上角像素坐标 (带 1 像素边框偏移)
    float2 probeTopLeft = float2(probeX, probeY) * (probeRes + 2.0) + 1.0;

    // 4. 将局部 [0, 1] 映射到该块的像素范围内
    float2 pixelPos = probeTopLeft + normalizedOctUV * probeRes;

    // 5. 转换到全局 UV 空间
    float2 atlasSize = float2(gridDims.x, gridDims.y * gridDims.z) * (probeRes + 2.0);
    return pixelPos / atlasSize;
}

uint3 GetProbeGridCoord(uint probeIndex, Vector3 gridDimensions)
{
    uint3 gridCoord;
    gridCoord.x = probeIndex % gridDimensions.x;
    gridCoord.y = (probeIndex / gridDimensions.x) % gridDimensions.y;
    gridCoord.z = probeIndex / (gridDimensions.x * gridDimensions.y);
    return gridCoord;
}
float3 GetProbeWorldPosition(uint probeIndex,
                             Vector3 gridOrigin,
                             Vector3 gridSpacing,
                             Vector3 gridDimensions)
{
    uint3 coord = GetProbeGridCoord(probeIndex, gridDimensions);
    // 位置 = 起点 + 索引 * 步长
    return gridOrigin + (float3(coord) * gridSpacing);
}

float3 SphericalFibonacci(uint sampleIndex, uint numSamples, float rotation)
{
    float b = (sqrt(5.0) * 0.5 + 0.5) - 1.0;
    float phi = 2.0 * 3.1415926f * b;

    float theta = phi * sampleIndex + rotation;
    float cosPhi = 1.0 - (float(sampleIndex) + 0.5) / float(numSamples) * 2.0;
    float sinPhi = sqrt(saturate(1.0 - cosPhi * cosPhi));

    return float3(cos(theta) * sinPhi, cosPhi, sin(theta) * sinPhi);
}
#endif