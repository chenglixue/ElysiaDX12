#ifndef DDGI_COMMON_H
#define DDGI_COMMON_H
#include "ShadingCommon.hlsl"
#include "private/Random.hlsl"

#define PROBE_COUNT 10648
#define RAYS_PER_PROBE 128
#define DDGI_PROBE_IRRADIANCE_NUM_TEXELS 8
#define DDGI_PROBE_DEPTH_NUM_TEXELS 16
#define DXR_MAX 10000
#define DXR_SHADOW_MAX 1e27f

#define PROBE_STATE_INACTIVE 0
#define PROBE_STATE_ACTIVE   1
#define RELOCATE_RAY_COUNT 32

struct Vertex
{
    float3 positionOS;
    float2 uv;
    float3 normalOS;
    float4 tangentOS;
    float4 color;
};

struct RayData
{
    Vector4 Position;
    Vector4 Data;
};
struct GIData
{
    Vector3 Irradiance;
    float Distance;
};

struct ShadowRayload
{
    bool isHit;
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

struct AABBData
{
    Vector3 Min;
    float pad0;
    Vector3 Max;
    float pad1;
};

Vertex InterpolateVertex(Vertex vertices[3], float3 barycentrics)
{
    Vertex o = (Vertex)0;

    for (UINT i = 0; i < 3; i ++)
    {
        o.positionOS += barycentrics * vertices[i].positionOS;
        o.normalOS += barycentrics * vertices[i].normalOS;
        o.tangentOS.xyz += barycentrics * vertices[i].tangentOS.xyz;
        o.uv += barycentrics * vertices[i].uv;
    }

    o.normalOS = normalize(o.normalOS);
    o.tangentOS.xyz = normalize(o.tangentOS.xyz);
    o.tangentOS.w = vertices[0].tangentOS.w;

    return o;
}

Vertex InterpolateVertex(Vertex v0, Vertex v1, Vertex v2, float3 barycentrics)
{
    Vertex o = (Vertex)0;

    o.positionOS += barycentrics * v0.positionOS;
    o.normalOS += barycentrics * v0.normalOS;
    o.tangentOS.xyz += barycentrics * v0.tangentOS.xyz;
    o.uv += barycentrics * v0.uv;
    o.color.rgb += barycentrics * v0.color.rgb;

    o.positionOS += barycentrics * v1.positionOS;
    o.normalOS += barycentrics * v1.normalOS;
    o.tangentOS.xyz += barycentrics * v1.tangentOS.xyz;
    o.uv += barycentrics * v1.uv;
    o.color.rgb += barycentrics * v1.color.rgb;

    o.positionOS += barycentrics * v2.positionOS;
    o.normalOS += barycentrics * v2.normalOS;
    o.tangentOS.xyz += barycentrics * v2.tangentOS.xyz;
    o.uv += barycentrics * v2.uv;
    o.color.rgb += barycentrics * v2.color.rgb;

    o.normalOS = normalize(o.normalOS);
    o.tangentOS.xyz = normalize(o.tangentOS.xyz);
    o.tangentOS.w = v0.tangentOS.w;

    return o;
}

bool IsPointInAABB(float3 position, AABBData aabb, float margin)
{
    // 将包围盒向外扩充一个 margin (通常是探针间距)
    return (position.x >= aabb.Min.x - margin && position.x <= aabb.Max.x + margin) &&
           (position.y >= aabb.Min.y - margin && position.y <= aabb.Max.y + margin) &&
           (position.z >= aabb.Min.z - margin && position.z <= aabb.Max.z + margin);
}

float2 SignNotZero(float2 v)
{
    return float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// 压缩：float3 (单位向量) -> uint (R16G16_UNORM 封装)
uint PackNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 res = (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * SignNotZero(n.xy);
    res = res * 0.5 + 0.5; // 映射到 [0, 1]

    // 将两个 float16 压入一个 uint
    uint x = uint(res.x * 65535.0);
    uint y = uint(res.y * 65535.0);
    return (x << 16) | y;
}

// 解压：uint -> float3 (单位向量)
float3 UnpackNormal(uint packed)
{
    float2 v = float2(float(packed >> 16) / 65535.0, float(packed & 0xFFFF) / 65535.0);
    v = v * 2.0 - 1.0; // 映射回 [-1, 1]

    float3 n = float3(v.x, v.y, 1.0 - abs(v.x) - abs(v.y));
    float t = saturate(-n.z);
    n.xy += (n.xy >= 0.0) ? -t : t;
    return normalize(n);
}

// 3D dir normalize to [-1, 1]
float2 OctEncode(float3 n)
{
    n *= rcp((abs(n.x) + abs(n.y) + abs(n.z)));
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
    uint probeY = probeIndex * rcp((uint)gridDims.x);

    // 3. 计算该探针 8x8 块的左上角像素坐标 (带 1 像素边框偏移)
    float2 probeTopLeft = float2(probeX, probeY) * (probeRes + 2.0) + 1.0;

    // 4. 将局部 [0, 1] 映射到该块的像素范围内
    float2 pixelPos = probeTopLeft + normalizedOctUV * probeRes;

    // 5. 转换到全局 UV 空间
    float2 atlasSize = float2(gridDims.x, gridDims.y * gridDims.z) * (probeRes + 2.0);
    return pixelPos * rcp(atlasSize);
}

uint3 GetProbeGridCoord(uint probeIndex, Vector3 gridDimensions)
{
    uint3 gridCoord;
    gridCoord.x = probeIndex % gridDimensions.x;
    gridCoord.y = (probeIndex * rcp(gridDimensions.x)) % gridDimensions.y;
    gridCoord.z = probeIndex * rcp(gridDimensions.x * gridDimensions.y);
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

float4 QuaternionFromAxisAngle(float3 axis, float angle)
{
    float s, c;
    sincos(angle * 0.5f, s, c);
    return float4(axis * s, c);
}

/**
 * Rotate vector v with quaternion q.
 */
float3 DDGIQuaternionRotate(float3 v, float4 q)
{
    float3 b = q.xyz;
    float b2 = dot(b, b);
    return (v * (q.w * q.w - b2) + b * (dot(v, b) * 2.f) + cross(b, v) * (q.w * 2.f));
}

/**
 * Quaternion conjugate.
 * For unit quaternions, conjugate equals inverse.
 * Use this to create a quaternion that rotates in the opposite direction.
 */
float4 DDGIQuaternionConjugate(float4 q)
{
    return float4(-q.xyz, q.w);
}

float3 SphericalFibonacci(uint sampleIndex, uint numSamples)
{
    float b = (sqrt(5.0) * 0.5 + 0.5) - 1.0;
    float phi = TWO_PI * b;

    float theta = phi * sampleIndex;
    float cosPhi = 1.0 - (float(sampleIndex) + 0.5) * rcp(float(numSamples)) * 2.0;
    float sinPhi = sqrt(saturate(1.0 - cosPhi * cosPhi));

    return float3(cos(theta) * sinPhi, cosPhi, sin(theta) * sinPhi);
}

float3 DDGIGetProbeRayDir(uint sampleIndex,
                          uint numSamples,
                          Vector4 randomRotation)
{
    bool isFixedRay = sampleIndex < RELOCATE_RAY_COUNT;
    sampleIndex = isFixedRay ? sampleIndex : sampleIndex - RELOCATE_RAY_COUNT;
    numSamples = isFixedRay
                     ? RELOCATE_RAY_COUNT
                     : numSamples - RELOCATE_RAY_COUNT;

    // 4. 判断是否为固定光线 (参考 NVIDIA 策略)
    // 0-31 条光线不参与随机旋转，用于几何定位
    float3 dir = SphericalFibonacci(sampleIndex, numSamples);
    [branch]
    if (isFixedRay)
    {
        return normalize(dir);
    }

    return normalize(DDGIQuaternionRotate(dir, DDGIQuaternionConjugate(randomRotation)));
}

/**
 * Returns the largest component of the vector.
 */
float DDGIMaxComponent(float3 a)
{
    return max(a.x, max(a.y, a.z));
}


float DDGILinearRGBToLuminance(float3 rgb)
{
    const float3 LuminanceWeights = float3(0.2126, 0.7152, 0.0722);
    return dot(rgb, LuminanceWeights);
}

float DDGI_Shadow_Visibity(float3 PositionWS,
                           float3 NormalWS,
                           float3 normalBias,
                           float3 ToLight,
                           RaytracingAccelerationStructure SceneTLAS)
{
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = PositionWS + NormalWS * normalBias;
    shadowRayDesc.Direction = ToLight;
    shadowRayDesc.TMin = 0.001f;
    shadowRayDesc.TMax = DXR_SHADOW_MAX;

    ShadowRayload shadowPayload;
    shadowPayload.isHit = true;

    TraceRay(SceneTLAS,
             // 找到第一个遮挡就停止
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
             // 只跑 AnyHit 或 Miss
             RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
             RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
             0xFF,
             0,
             1,
             0,
             shadowRayDesc,
             shadowPayload
        );

    return shadowPayload.isHit ? 0.f : 1.f;
}

float DDGI_Query_Shadow_Visibity(float3 PositionWS,
                                 float3 NormalWS,
                                 float3 normalBias,
                                 float3 ToLight,
                                 RaytracingAccelerationStructure SceneTLAS)
{
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = PositionWS + NormalWS * normalBias;
    shadowRayDesc.Direction = ToLight;
    shadowRayDesc.TMin = 0.001f;
    shadowRayDesc.TMax = DXR_SHADOW_MAX;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

    q.TraceRayInline(SceneTLAS, RAY_FLAG_NONE, 0xFF, shadowRayDesc);
    while (q.Proceed());

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        return 0.f;
    }

    return 1.f;
}

float DDGIGetVolumeBlendWeight(float3 positionWS,
                               float3 gridOrigin,
                               float3 gridSpacing,
                               int3 probeScrollOffsets,
                               float4 rotation)
{
    // Get the volume's origin and extent
    float3 origin = gridOrigin + (probeScrollOffsets * gridSpacing);
    float3 extent = (gridSpacing * (PROBE_COUNT - 1)) * 0.5f;

    // Get the delta between the (rotated volume) and the world-space position
    float3 position = (positionWS - origin);
    position = abs(DDGIQuaternionRotate(position, DDGIQuaternionConjugate(rotation)));

    float3 delta = position - extent;
    if (all(delta < 0))
        return 1.f;

    // Adjust the blend weight for each axis
    float volumeBlendWeight = 1.f;
    volumeBlendWeight *= (1.f - saturate(delta.x * rcp(gridSpacing.x)));
    volumeBlendWeight *= (1.f - saturate(delta.y * rcp(gridSpacing.y)));
    volumeBlendWeight *= (1.f - saturate(delta.z * rcp(gridSpacing.z)));

    return volumeBlendWeight;
}
#endif