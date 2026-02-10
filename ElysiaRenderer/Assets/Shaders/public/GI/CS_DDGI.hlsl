#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridSpacing;
    UINT g_ProbeOffsetsIndex;
    UINT g_RayDataBufferIndex;
    float g_RandomRotation;
}

static const float PROBE_MIN_FRONTFACE_DIST = 0.6f; // 保持距离墙面 0.2 单位
static const float PROBE_RETURN_HOME_HYSTERESIS = 0.05f;
static const float PROBE_BACKFACE_THRESHOLD = 0.25f;  // % 射线撞背面视为在内部
static const float PROBE_MAX_OFFSET_FRACTION = 0.45f; // 最大允许偏移量 (相对于Grid间距的比例, 0.5是边界, 0.45是安全区)

[numthreads(GROUP_SIZE * GROUP_SIZE, 1, 1)]
void ClearProbeOffsetBuffer(uint3 id : SV_DispatchThreadID)
{
    uint probeIndex = id.x;
    if (probeIndex >= PROBE_COUNT)
        return;
    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    probeOffsetBuffer[probeIndex] = 0.f;
}

[numthreads(GROUP_SIZE * GROUP_SIZE, 1, 1)]
void RelocateProbes(uint3 id : SV_DispatchThreadID)
{
    uint probeIndex = id.x;
    if (probeIndex >= PROBE_COUNT)
        return;

    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    StructuredBuffer<RayData> rayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];

    int closestBackfaceIndex = -1;
    int closestFrontfaceIndex = -1;
    int farthestFrontfaceIndex = -1;
    float backFaceCount = 0;
    float closestBackfaceRealDist = 0.0f;
    float closestBackfaceScaledDist = 1e27f;
    float closestFrontfaceDist = 1e27f;
    float farthestFrontfaceDist = 0.0f;

    for (UINT i = 0; i < Rays_Per_Probe; ++i)
    {
        UINT rayIndex = probeIndex * Rays_Per_Probe + i;
        RayData rayData = rayDataBuffer[rayIndex];
        float dist = rayData.Distance;
        // float3 dir = SphericalFibonacci(i, Rays_Per_Probe, g_RandomRotation);

        if (dist < 0.f)
        {
            backFaceCount += 1.0f;
            float scaledDist = dist * -5.0f;
            if (scaledDist < closestBackfaceScaledDist)
            {
                closestBackfaceScaledDist = scaledDist;
                closestBackfaceIndex = i;
                closestBackfaceRealDist = abs(dist);
            }
        }
        else
        {
            if (dist < closestFrontfaceDist)
            {
                closestFrontfaceDist = dist;
                closestFrontfaceIndex = i;
            }
            if (dist > farthestFrontfaceDist)
            {
                farthestFrontfaceDist = dist;
                farthestFrontfaceIndex = i;
            }
        }
    }

    float3 currentOffset = probeOffsetBuffer[probeIndex];
    float3 targetOffset = float3(1e27f, 1e27f, 1e27f); // 哨兵值

    // 2. 决策阶段 (Logic Phase)
    // === 逻辑 A: 穿墙逃逸 (Punch Through) ===
    // 如果背面击中过多，说明在内部。
    if (closestBackfaceIndex != -1 && (backFaceCount / Rays_Per_Probe) > PROBE_BACKFACE_THRESHOLD)
    {
        float3 backfaceDir = SphericalFibonacci(closestBackfaceIndex,
                                                Rays_Per_Probe,
                                                g_RandomRotation);
        float escapeDist = closestBackfaceRealDist + PROBE_MIN_FRONTFACE_DIST + 0.05f;
        targetOffset = currentOffset + (backfaceDir * escapeDist);
    }
    // === 逻辑 B: 寻找空地 (Avoid Clutter) ===
    else if (closestFrontfaceIndex != -1 && closestFrontfaceDist < PROBE_MIN_FRONTFACE_DIST)
    {
        float3 closeDir = SphericalFibonacci(closestFrontfaceIndex,
                                             Rays_Per_Probe,
                                             g_RandomRotation);
        float3 farDir =
            SphericalFibonacci(farthestFrontfaceIndex, Rays_Per_Probe, g_RandomRotation);

        if (dot(closeDir, farDir) <= 0.0f)
        {
            targetOffset = currentOffset + (farDir * min(farthestFrontfaceDist, 1.0f));
        }
    }
    // === 逻辑 C: 回家 (Return Home) - 修复抖动的关键 ===
    // 只有当距离 大于 (最小安全距离 + 滞后死区) 时，才允许被拉回。
    // 例如：只有距离 > 0.25m 时，才会被拉回。
    // 如果距离在 0.20m ~ 0.25m 之间，既不推也不拉，保持静止！
    else if (closestFrontfaceDist > (PROBE_MIN_FRONTFACE_DIST + PROBE_RETURN_HOME_HYSTERESIS))
    {
        if (length(currentOffset) > 0.001f)
        {
            // 注意：我们只把探针拉回到死区边缘，而不是拉回到 0.2
            // 这样可以防止拉过头
            float safeDist = PROBE_MIN_FRONTFACE_DIST + PROBE_RETURN_HOME_HYSTERESIS;
            float moveBackMargin = min(closestFrontfaceDist - safeDist, length(currentOffset));

            float3 moveBackDir = normalize(-currentOffset);
            targetOffset = currentOffset + (moveBackDir * moveBackMargin);
        }
    }

    // 3. 验证阶段 (Conservative Validation)

    // 只有当计算出了新的 targetOffset 且不为哨兵值时，才应用。
    if (targetOffset.x < 1e26f)
    {
        float3 normalized = targetOffset / g_GridSpacing.xyz;
        if (dot(normalized, normalized) < (PROBE_MAX_OFFSET_FRACTION * PROBE_MAX_OFFSET_FRACTION))
        {
            probeOffsetBuffer[probeIndex] = targetOffset;
        }
    }
}