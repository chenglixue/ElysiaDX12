#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridSpacing;
    float3 g_GridDimensions;
    UINT g_ProbeOffsetsIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    float g_RandomRotation;
    float g_DDGIBlendWeight;
    float g_ProbeIrradianceThreshold;
    float g_ProbeBrightnessThreshold;
    float g_DDGIEncodingGamma;
}

static const float PROBE_MIN_FRONTFACE_DIST = 0.3f; // 距离墙面多远
static const float PROBE_RETURN_HOME_HYSTERESIS = 0.f;
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

void Elysia_DDGI_StoreDist(uint2 id, float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[g_DistanceTexIndex];
    o[id].rg = val;
}
float2 Elysia_DDGI_LoadDist(uint2 id)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[g_DistanceTexIndex];
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

    for (UINT i = 0; i < RAYS_PER_PROBE; ++i)
    {
        UINT rayIndex = probeIndex * RAYS_PER_PROBE + i;
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
    if (closestBackfaceIndex != -1 && (backFaceCount / RAYS_PER_PROBE) > PROBE_BACKFACE_THRESHOLD)
    {
        float3 backfaceDir = SphericalFibonacci(closestBackfaceIndex,
                                                RAYS_PER_PROBE,
                                                g_RandomRotation);
        float escapeDist = closestBackfaceRealDist + PROBE_MIN_FRONTFACE_DIST * 0.5f;
        targetOffset = currentOffset + (backfaceDir * escapeDist);
    }
    // === 逻辑 B: 寻找空地 (Avoid Clutter) ===
    else if (closestFrontfaceDist < PROBE_MIN_FRONTFACE_DIST)
    {
        float3 closeDir = SphericalFibonacci(closestFrontfaceIndex,
                                             RAYS_PER_PROBE,
                                             g_RandomRotation);
        float3 farDir =
            SphericalFibonacci(farthestFrontfaceIndex, RAYS_PER_PROBE, g_RandomRotation);

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

[numthreads(DDGI_PROBE_NUM_TEXELS, DDGI_PROBE_NUM_TEXELS, 1)]
void ProbeBlending(uint3 id : SV_DispatchThreadID,
                   uint3 GroupThreadID : SV_GroupThreadID,
                   uint3 GroupID : SV_GroupID)
{
    UINT probeIndex = GroupID.x + (GroupID.y * g_GridDimensions.x);
    uint N = DDGI_PROBE_NUM_TEXELS;

    if (probeIndex >= PROBE_COUNT || probeIndex < 0)
        return;

    // 将 [1, 6] 映射到八面体坐标的 [-1, 1]
    bool isBorder = (GroupThreadID.x == 0 || GroupThreadID.x == (DDGI_PROBE_NUM_TEXELS - 1) ||
                     GroupThreadID.y == 0 || GroupThreadID.y == (DDGI_PROBE_NUM_TEXELS - 1));
    if (!isBorder)
    {
        float2 uv = (float2(GroupThreadID.xy) - 1.f + 0.5f) / (float)(DDGI_PROBE_NUM_TEXELS - 2);
        float2 octUV = uv * 2.0f - 1.0f;
        float3 probeDirection = OctDecode(octUV);

        float4 accumulatedIrradiance = 0.0f;
        float2 accumulatedDist = 0.0f;
        float distSumWeight = 0.f;
        float probeMaxRayDistance = length(g_GridSpacing) * 1.5f;
        for (int r = 0; r < RAYS_PER_PROBE; r ++)
        {
            float3 rayDir = SphericalFibonacci(r, RAYS_PER_PROBE, g_RandomRotation);
            RayData rayData = Elysia_DDGI_LoadRayData(probeIndex * RAYS_PER_PROBE + r);

            // 方向越接近，权重越高
            float weight = max(0.f, dot(probeDirection, rayDir));

            // 只处理正面碰撞
            if (rayData.Distance >= 0.f && weight > 0.f)
            {
                // 对于 Irradiance，累加 (Radiance * w, w)
                accumulatedIrradiance += float4(rayData.Radiance * weight, weight);
            }

            if (weight > 0.f)
            {
                // NVIDIA 建议：距离权重的指数通常更高（如 16.0），这能让遮挡判定更锐利
                float distWeight = pow(weight, 50.0f);
                float absDist = min(abs(rayData.Distance), probeMaxRayDistance);

                accumulatedDist += float2(absDist * distWeight, (absDist * absDist) * distWeight);
                distSumWeight += distWeight;
            }
        }
        float epsilon = float(RAYS_PER_PROBE) * 1e-9f;
        float invGamma = 1.0f / g_DDGIEncodingGamma;

        // NVIDIA 建议除以 (2.0 * sumWeight) 以匹配漫反射积分
        float3 netIrradiance = accumulatedIrradiance.rgb /
                               (2.0f * max(accumulatedIrradiance.a, epsilon));
        netIrradiance = pow(netIrradiance, invGamma);
        float2 netDist = accumulatedDist / max(distSumWeight, epsilon);

        float4 historyIrradiance = Elysia_DDGI_LoadIrradiance(id.xy);
        float2 historyDist = Elysia_DDGI_LoadDist(id.xy);
        float hysteresis = saturate(g_DDGIBlendWeight); // 历史权重
        float3 delta = (netIrradiance - historyIrradiance);

        // 如果历史是黑的，直接覆盖（防止冷启动过慢）
        if (dot(historyIrradiance, historyIrradiance) == 0.0f)
        {
            hysteresis = 0.0f;
        }

        // 亮度剧变检测 (Irradiance Thresholding)
        // 如果新旧颜色分量差异过大，认为光源发生了剧烈移动，强行降低滞后，加快刷新
        if (DDGIMaxComponent(historyIrradiance - netIrradiance) > g_ProbeIrradianceThreshold)
        {
            hysteresis = max(0.0f, hysteresis - 0.75f); // 瞬间变得非常“敏锐”
        }

        // 亮度增幅限制 (Brightness Thresholding)
        // 防止由于 Ray Tracing 噪声产生的极亮像素导致探针闪烁
        float luminanceDelta = DDGILinearRGBToLuminance(historyIrradiance - netIrradiance);
        if (luminanceDelta > g_ProbeBrightnessThreshold)
        {
            delta *= 0.25f; // 限制本次更新的步长
        }

        // 能量收敛优化 (Darkening Convergence)
        // 解决在低亮度下（UNORM 格式）由于混合精度不足导致的颜色“卡住”不消失的问题
        static const float c_threshold = 1.f / 1024.f;
        float3 lerpDelta = (1.f - hysteresis) * delta;
        // 如果是在变暗，确保至少步进一个最小单位
        if (DDGIMaxComponent(netIrradiance) < DDGIMaxComponent(historyIrradiance))
        {
            lerpDelta = min(max(c_threshold, abs(lerpDelta)), abs(delta)) * sign(lerpDelta);
        }

        float2 distDelta = (netDist - historyDist);

        float3 finalColor = historyIrradiance + lerpDelta;
        float2 finalDist = historyDist + (1.0f - hysteresis) * distDelta; // 共享由亮度触发的 hysteresis
        Elysia_DDGI_StoreIrradiance(id.xy, finalColor);
        Elysia_DDGI_StoreDist(id.xy, finalDist);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isBorder)
    {
        uint2 localCoord = GroupThreadID.xy;
        uint2 copyLocalCoord;

        // 2. 计算八面体映射的镜像拷贝坐标
        // 处理四个角 (Corners)：对角线镜像
        bool isCorner = ((localCoord.x == 0 || localCoord.x == N - 1) &&
                         (localCoord.y == 0 || localCoord.y == N - 1));

        if (isCorner)
        {
            // 角像素拷贝自其对角线上最远的内部像素
            copyLocalCoord.x = (localCoord.x == 0) ? N - 2 : 1;
            copyLocalCoord.y = (localCoord.y == 0) ? N - 2 : 1;
        }
        else if (localCoord.x == 0 || localCoord.x == (N - 1))
        {
            // 左右边界：纵向镜像拷贝
            copyLocalCoord.x = (localCoord.x == 0) ? N - 2 : 1;
            copyLocalCoord.y = (N - 1) - localCoord.y;
        }
        else // 上下边界 (localCoord.y == 0 || localCoord.y == N - 1)
        {
            // 上下边界：横向镜像拷贝
            copyLocalCoord.y = (localCoord.y == 0) ? N - 2 : 1;
            copyLocalCoord.x = (N - 1) - localCoord.x;
        }

        // 3. 计算全局采样坐标并写入
        uint2 globalCopyPos = GroupID.xy * N + copyLocalCoord;

        // 同时缝合 Irradiance 和 Distance
        float3 borderIrr = Elysia_DDGI_LoadIrradiance(globalCopyPos).rgb;
        float2 borderDist = Elysia_DDGI_LoadDist(globalCopyPos).rg;

        Elysia_DDGI_StoreIrradiance(id.xy, borderIrr);
        Elysia_DDGI_StoreDist(id.xy, borderDist);
    }

}