#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridSpacing;
    UINT3 g_GridDimensions;
    UINT g_ProbeOffsetsIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    float g_RandomRotation;
}

static const float PROBE_MIN_FRONTFACE_DIST = 0.4f; // 保持距离墙面 0.2 单位
static const float PROBE_RETURN_HOME_HYSTERESIS = 0.05f;
static const float PROBE_BACKFACE_THRESHOLD = 0.25f;  // % 射线撞背面视为在内部
static const float PROBE_MAX_OFFSET_FRACTION = 0.45f; // 最大允许偏移量 (相对于Grid间距的比例, 0.5是边界, 0.45是安全区)

void Elysia_DDGI_StoreRayData(uint writeIndex, float3 radiance, float distance)
{
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    rayDatas[writeIndex].Radiance = radiance;
    rayDatas[writeIndex].Distance = distance;
}
RayData Elysia_DDGI_LoadRayData(uint readIndex)
{
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    return rayDatas[readIndex];
}

void Elysia_DDGI_StoreIrradiance(uint2 id, float3 val)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_IrradianceTexIndex];
    o[id].rgb = val;
}
float4 Elysia_DDGI_LoadIrradiance(uint2 id)
{
    RWTexture2D<float4> o = ResourceDescriptorHeap[g_IrradianceTexIndex];
    return o[id];
}

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

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ProbeBlending(uint3 id : SV_DispatchThreadID,
                   uint3 GroupThreadID : SV_GroupThreadID,
                   uint3 GroupID : SV_GroupID)
{
    UINT probeIndex = GroupID.x + (GroupID.y * g_GridDimensions.x);

    // 将 [1, 6] 映射到八面体坐标的 [-1, 1]
    bool isBorder = (GroupThreadID.x == 0 || GroupThreadID.x == 7 ||
                     GroupThreadID.y == 0 || GroupThreadID.y == 7);
    if (!isBorder)
    {
        float2 uv = (float2(GroupThreadID.xy) - 1.f + 0.5f) / (float)(GROUP_SIZE - 2);
        float2 octUV = uv * 2.0f - 1.0f;
        float3 probeDirection = OctDecode(octUV);

        float4 accumulatedResult = 0.0f;
        for (int r = 0; r < Rays_Per_Probe; r ++)
        {
            float3 rayDir = SphericalFibonacci(r, Rays_Per_Probe, g_RandomRotation);
            RayData rayData = Elysia_DDGI_LoadRayData(probeIndex * Rays_Per_Probe + r);

            // 只处理正面碰撞
            if (rayData.Distance >= 0.f)
            {
                // 方向越接近，权重越高
                float weight = max(0.f, dot(probeDirection, rayDir));
                if (weight > 0.0f)
                {
                    // 对于 Irradiance，累加 (Radiance * w, w)
                    accumulatedResult += float4(rayData.Radiance * weight, weight);
                }
            }
        }

        // NVIDIA 建议除以 (2.0 * sumWeight) 以匹配漫反射积分
        float3 netIrradiance = accumulatedResult.rgb / (2.0f * max(accumulatedResult.a, 1e-6f));
        float3 history = Elysia_DDGI_LoadIrradiance(id.xy).rgb;
        float hysteresis = 0.97f; // 历史权重

        // 如果历史是黑的，直接覆盖（防止冷启动过慢）
        if (dot(history, history) == 0.0f)
            hysteresis = 0.0f;

        float3 finalColor = lerp(netIrradiance, history, hysteresis);
        Elysia_DDGI_StoreIrradiance(id.xy, finalColor);
    }

}