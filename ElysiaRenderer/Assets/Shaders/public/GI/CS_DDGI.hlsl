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

static const float PROBE_MIN_FRONTFACE_DIST = 0.3f;   // 距离墙面多远
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
    uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);

    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    StructuredBuffer<RayData> rayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];

    int closestBackfaceIndex = -1;
    int closestFrontfaceIndex = -1;
    int farthestFrontfaceIndex = -1;
    float backFaceCount = 0;
    float closestBackfaceDist = 1e27f;
    float closestFrontfaceDist = 1e27f;
    float farthestFrontfaceDist = 0.0f;

    const uint RELOCATE_RAY_COUNT = 32;
    for (UINT i = 0; i < RELOCATE_RAY_COUNT; ++i)
    {
        UINT rayIndex = probeIndex * RAYS_PER_PROBE + i;
        RayData rayData = rayDataBuffer[rayIndex];
        float hitDistance = rayData.Distance;

        if (hitDistance < 0.f)
        {
            backFaceCount += 1.0f;
            hitDistance = abs(hitDistance) * 5.0f;
            if (hitDistance < closestBackfaceDist)
            {
                closestBackfaceIndex = (int)i;
                closestBackfaceDist = hitDistance;
            }
        }
        else
        {
            if (hitDistance < closestFrontfaceDist)
            {
                closestFrontfaceDist = hitDistance;
                closestFrontfaceIndex = i;
            }
            if (hitDistance > farthestFrontfaceDist)
            {
                farthestFrontfaceDist = hitDistance;
                farthestFrontfaceIndex = i;
            }
        }
    }

    float3 currentOffset = probeOffsetBuffer[probeIndex];
    float3 targetOffset = float3(FLT_INF, FLT_INF, FLT_INF);
    // A maximum offset computed from the probe grid spacing
    float3 offsetLimit = g_GridSpacing.xyz * PROBE_MAX_OFFSET_FRACTION;

    // If there’s a close backface AND you see more than 25% backfaces, assume you’re inside something.
    if (closestBackfaceIndex != -1 && (backFaceCount / RELOCATE_RAY_COUNT) > PROBE_BACKFACE_THRESHOLD)
    {
        // direction to closest backface scaled by distance
        float3 backfaceDir = DDGIGetProbeRayDir(closestBackfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex, true);
        float3 closestBackfaceVector = backfaceDir * closestBackfaceDist;

        float scaleFactor = 2.0;
        [unroll]
        for (int i = 1; i <= 100; ++i)
        {
            if (!all(abs(targetOffset) < offsetLimit))
            {
                targetOffset = currentOffset + closestBackfaceVector * (scaleFactor - i * 0.01f);
            }
        }
    }
    // === 逻辑 B: 寻找空地 (Avoid Clutter) ===
    // else if (closestFrontfaceDist < PROBE_MIN_FRONTFACE_DIST)
    else if (closestFrontfaceIndex != -1 && farthestFrontfaceIndex != -1)
    {
        // direction to closest frontface scaled by distance
        float3 closeDir = DDGIGetProbeRayDir(closestFrontfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex, true);
        // direction to farthest frontface scaled by distance
        float3 farDir = DDGIGetProbeRayDir(farthestFrontfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex, true);

        if (!(dot(closeDir, farDir) > 0.5f))
        {
            float3 farestDir = min(0.2f, farthestFrontfaceDist) * farDir;
            targetOffset = currentOffset + farestDir;
        }
    }

    if (all(abs(targetOffset) < offsetLimit))
    {
        probeOffsetBuffer[probeIndex] = targetOffset;
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

    uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);
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
            float3 rayDir = DDGIGetProbeRayDir(r, RAYS_PER_PROBE, gridIdx, frameIndex, false);
            RayData rayData = Elysia_DDGI_LoadRayData(probeIndex * RAYS_PER_PROBE + r);

            if (rayData.Distance < 0.0f)
            {
                rayData.Radiance = 0.0f;
                // 深度缩短 80%：强制让均值和方差向“极近距离”偏移，增强遮挡判定
                rayData.Distance = abs(rayData.Distance) * 0.2f;
            }

            float3 radiance = rayData.Radiance;
            // float rayLuma = dot(radiance, float3(0.2126, 0.7152, 0.0722));
            // float maxRayBrightness = 10.0f; // 这里的阈值可以根据场景曝光调整
            // if (rayLuma > maxRayBrightness)
            // {
            //     radiance *= (maxRayBrightness / rayLuma);
            // }

            // 方向越接近，权重越高
            float weight = max(0.f, dot(probeDirection, rayDir));
            // 只处理正面碰撞
            if (weight > 0.f)
            {
                // 对于 Irradiance，累加 (Radiance * w, w)
                accumulatedIrradiance += float4(radiance * weight, weight);

                float distWeight = pow(weight, 50.0f);
                float absDist = min(abs(rayData.Distance), probeMaxRayDistance);

                accumulatedDist += float2(absDist * distWeight, (absDist * absDist) * distWeight);
                distSumWeight += distWeight;
            }
        }
        float epsilon = float(RAYS_PER_PROBE) * 1e-9f;

        // NVIDIA 建议除以 (2.0 * sumWeight) 以匹配漫反射积分
        float3 netIrradiance = accumulatedIrradiance.rgb /
                               (2.0f * max(accumulatedIrradiance.a, epsilon));
        netIrradiance = pow(netIrradiance, 1.0f / g_DDGIEncodingGamma);
        float2 netDist = accumulatedDist / max(distSumWeight, epsilon);

        float4 historyIrradiance = Elysia_DDGI_LoadIrradiance(id.xy);
        float2 historyDist = Elysia_DDGI_LoadDist(id.xy);
        float hysteresis = saturate(g_DDGIBlendWeight); // 历史权重
        float distHysteresis = hysteresis;
        float3 delta = (netIrradiance - historyIrradiance);

        // 如果历史是黑的，直接覆盖（防止冷启动过慢）
        if (dot(historyIrradiance, historyIrradiance) == 0.0f)
        {
            hysteresis = 0.0f;
        }

        float significantChangeThreshold = 0.25f;
        float newDistributionChangeThreshold = 0.8f;

        float changeMagnitude = DDGIMaxComponent(abs(netIrradiance - historyIrradiance));
        if (changeMagnitude > significantChangeThreshold)
        {
            hysteresis = max(0, hysteresis - 0.15f);
        }
        if (changeMagnitude > newDistributionChangeThreshold)
        {
            hysteresis = 0.f;
        }

        // 亮度剧变检测 (Irradiance Thresholding)
        // 如果新旧颜色分量差异过大，认为光源发生了剧烈移动，强行降低滞后，加快刷新
        // if (DDGIMaxComponent(historyIrradiance - netIrradiance) > g_ProbeIrradianceThreshold)
        // {
        //     hysteresis = max(0.0f, hysteresis - 0.75f); // 瞬间变得非常“敏锐”
        // }

        // 亮度增幅限制 (Brightness Thresholding)
        // 防止由于 Ray Tracing 噪声产生的极亮像素导致探针闪烁
        float luminanceDelta = DDGILinearRGBToLuminance(abs(historyIrradiance - netIrradiance));
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
        float2 finalDist = historyDist + (1.0f - distHysteresis) * distDelta; // 共享由亮度触发的 hysteresis
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