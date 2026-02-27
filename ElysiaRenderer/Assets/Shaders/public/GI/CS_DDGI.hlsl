#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_GridOrigin;
    float4 g_GridSpacing;
    float4 g_GridDimensions;
    UINT g_ProbeOffsetsIndex;
    UINT g_ProbeStatesIndex;
    UINT g_ProbeFrameBufferIndex;
    UINT g_StaticAABBIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    float g_RandomRotation;
    float g_DDGIBlendWeight;
    float g_ProbeIrradianceThreshold;
    float g_ProbeBrightnessThreshold;
    float g_DDGIEncodingGamma;
    UINT g_StaticAABBCount;
}

static const float PROBE_BACKFACE_THRESHOLD = 0.25f;  // % 射线撞背面视为在内部
static const float PROBE_MAX_OFFSET_FRACTION = 0.45f; // 最大允许偏移量 (相对于Grid间距的比例, 0.5是边界, 0.45是安全区)


AABBData Elysia_DDGI_LoadeStaticAABB(UINT id)
{
    RWStructuredBuffer<AABBData> o = ResourceDescriptorHeap[g_StaticAABBIndex];
    return o[id];
}

void Elysia_DDGI_StoreProbeState(UINT id, UINT state)
{
    RWStructuredBuffer<UINT> o = ResourceDescriptorHeap[g_ProbeStatesIndex];
    o[id] = state;
}
UINT Elysia_DDGI_LoadeProbeState(UINT id)
{
    RWStructuredBuffer<UINT> o = ResourceDescriptorHeap[g_ProbeStatesIndex];
    return o[id];
}

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
void ResetProbeStates(uint3 id : SV_DispatchThreadID)
{
    Elysia_DDGI_StoreProbeState(id.x, PROBE_STATE_ACTIVE);
}

[numthreads(GROUP_SIZE * GROUP_SIZE, 1, 1)]
void UpdateProbeStates(uint3 id : SV_DispatchThreadID)
{
    uint probeIndex = id.x;
    if (probeIndex >= PROBE_COUNT)
        return;

    int backfaceCount = 0;
    float hitDistances[RELOCATE_RAY_COUNT];
    StructuredBuffer<RayData> rayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];
    RWStructuredBuffer<UINT> probeStateBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];

    for (int rayIndex = 0; rayIndex < RELOCATE_RAY_COUNT; rayIndex ++)
    {
        // Get the coordinates for the probe ray in the RayData texture array
        RayData rayData = rayDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex];

        // Load the hit distance for the ray
        hitDistances[rayIndex] = rayData.Distance;

        // Increment the count if a backface is hit
        backfaceCount += (hitDistances[rayIndex] < 0.f);
    }
    if ((float)backfaceCount / (float)RELOCATE_RAY_COUNT > PROBE_BACKFACE_THRESHOLD)
    {
        probeStateBuffer[probeIndex] = PROBE_STATE_INACTIVE;
        return;
    }

    float3 probePosWS = GetProbeWorldPosition(probeIndex, g_GridOrigin, g_GridSpacing, g_GridDimensions);
    float probeSpacingMax = max(g_GridSpacing.x, max(g_GridSpacing.y, g_GridSpacing.z));

    bool isNearStatic = false;
    for (UINT i = 0; i < g_StaticAABBCount; ++i)
    {
        AABBData AABB = Elysia_DDGI_LoadeStaticAABB(i);
        if (IsPointInAABB(probePosWS, AABB, probeSpacingMax))
        {
            isNearStatic = true;
            break;
        }
    }

    if (isNearStatic)
    {
        Elysia_DDGI_StoreProbeState(probeIndex, PROBE_STATE_ACTIVE);
        return;
    }

    Elysia_DDGI_StoreProbeState(probeIndex, PROBE_STATE_INACTIVE);

    // dynamic state
    // if (currentState == PROBE_STATE_SLEEPING)
    // {
    //     // 论文要求：扩展 AABB = 探针网格单元 + 自阴影偏移量
    //     float dynamicMargin = probeSpacingMax + (0.75f * probeSpacingMax * g_TunableShadowBias);
    //
    //     for (uint j = 0; j < DynamicAABBCount; ++j)
    //     {
    //         if (IsPointInExpandedAABB(probeWorldPos, DynamicAABBs[j], dynamicMargin))
    //         {
    //             ProbeStateBuffer[probeIndex] = PROBE_STATE_NEWLY_AWAKE;
    //             break;
    //         }
    //     }
    // }
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
void RelocateProbes(UINT3 id : SV_DispatchThreadID)
{
    UINT probeIndex = id.x;
    if (probeIndex >= PROBE_COUNT || probeIndex < 0)
        return;
    UINT3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);

    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    StructuredBuffer<RayData> rayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];
    RWStructuredBuffer<UINT> probeStateBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
    RWStructuredBuffer<UINT> probeFrameBuffer = ResourceDescriptorHeap[g_ProbeFrameBufferIndex];

    int closestBackfaceIndex = -1;
    int closestFrontfaceIndex = -1;
    int farthestFrontfaceIndex = -1;
    float backFaceCount = 0;
    float closestBackfaceDist = 1e27f;
    float closestFrontfaceDist = 1e27f;
    float farthestFrontfaceDist = 0.0f;

    for (UINT i = 0; i < RELOCATE_RAY_COUNT; ++i)
    {
        UINT rayIndex = probeIndex * RAYS_PER_PROBE + i;
        RayData rayData = rayDataBuffer[rayIndex];
        float hitDistance = rayData.Distance;

        if (hitDistance < 0.f)
        {
            backFaceCount += 1.0f;
            // 还原物理距离 (RayGen中可能缩短了80%)
            hitDistance = (hitDistance) * -5.0f;
            if (hitDistance < closestBackfaceDist)
            {
                closestBackfaceIndex = i;
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
    float3 targetOffset = currentOffset;
    // A maximum offset computed from the probe grid spacing
    float3 offsetLimit = g_GridSpacing.xyz * PROBE_MAX_OFFSET_FRACTION;

    // direction to closest frontface scaled by distance
    float3 closeDir = DDGIGetProbeRayDir(closestFrontfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex);
    // direction to farthest frontface scaled by distance
    float3 farDir = DDGIGetProbeRayDir(farthestFrontfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex);

    // If there’s a close backface AND you see more than 25% backfaces, assume you’re inside something.
    if (closestBackfaceIndex != -1 && (backFaceCount / RELOCATE_RAY_COUNT) > PROBE_BACKFACE_THRESHOLD)
    {
        // direction to closest backface scaled by distance
        float3 backfaceDir = DDGIGetProbeRayDir(closestBackfaceIndex, RAYS_PER_PROBE, gridIdx, frameIndex);

        float scaleFactor = 2.0;
        [unroll]
        for (int i = 1; i <= 100; ++i)
        {
            float3 testOffset = currentOffset + backfaceDir * (scaleFactor - i * 0.01f);
            if (all(abs(testOffset) < offsetLimit))
            {
                targetOffset = testOffset;
                break;
            }
        }
    }
    // === 逻辑 B: 寻找空地 (Avoid Clutter) ===
    else if (closestFrontfaceIndex != -1 && farthestFrontfaceIndex != -1)
    {
        float minSafeDist = length(g_GridSpacing) * 0.2f;

        if (closestFrontfaceDist < minSafeDist)
        {

            if (!(dot(closeDir, farDir) > 0.5f))
            {
                float moveStep = min(0.2f, farthestFrontfaceDist * 0.5f);
                float3 farestDir = moveStep * farDir;
                targetOffset = currentOffset + farestDir;
            }
        }
    }

    float moveDist = length(targetOffset - currentOffset);
    float minMoveThreshold = length(g_GridSpacing) * 0.001f; // 极小的死区防止微小震荡

    // 只要有明显的位移趋势，就执行平滑移动
    if (moveDist > minMoveThreshold)
    {
        probeOffsetBuffer[probeIndex] = targetOffset;
    }

    // if (probeFrameBuffer[probeIndex] < 5)
    // {
    //     probeFrameBuffer[probeIndex] += 1;
    //     if (probeFrameBuffer[probeIndex] >= 5)
    //     {
    //         // still in wall -> OFF
    //         if ((backFaceCount / RELOCATE_RAY_COUNT) > PROBE_BACKFACE_THRESHOLD)
    //         {
    //             probeStateBuffer[probeIndex] = PROBE_STATE_INACTIVE;
    //             probeFrameBuffer[probeIndex] = 255;
    //         }
    //         else
    //         {
    //             probeStateBuffer[probeIndex] = PROBE_STATE_ACTIVE;
    //         }
    //     }
    // }
}

[numthreads(DDGI_PROBE_NUM_TEXELS, DDGI_PROBE_NUM_TEXELS, 1)]
void ProbeBlending(uint3 id : SV_DispatchThreadID,
                   uint3 GroupThreadID : SV_GroupThreadID,
                   uint3 GroupID : SV_GroupID)
{
    UINT probeIndex = GroupID.x + (GroupID.y * g_GridDimensions.x);
    uint N = DDGI_PROBE_NUM_TEXELS;

    UINT currProbeState = Elysia_DDGI_LoadeProbeState(probeIndex);

    if (probeIndex >= PROBE_COUNT || probeIndex < 0)
        return;

    uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);
    // 将 [1, 6] 映射到八面体坐标的 [-1, 1]
    bool isBorder = (GroupThreadID.x == 0 || GroupThreadID.x == (DDGI_PROBE_NUM_TEXELS - 1) ||
                     GroupThreadID.y == 0 || GroupThreadID.y == (DDGI_PROBE_NUM_TEXELS - 1));
    if (!isBorder)
    {
        if (currProbeState == PROBE_STATE_INACTIVE)
            return;
        float2 uv = (float2(GroupThreadID.xy) - 1.f + 0.5f) / (float)(DDGI_PROBE_NUM_TEXELS - 2);
        float2 octUV = uv * 2.0f - 1.0f;
        float3 probeDirection = OctDecode(octUV);

        float4 accumulatedIrradiance = 0.0f;
        float2 accumulatedDist = 0.0f;
        float distSumWeight = 0.f;
        float probeMaxRayDistance = length(g_GridSpacing) * 1.5f;
        for (int rayIndex = 32; rayIndex < RAYS_PER_PROBE; rayIndex ++)
        {
            float3 rayDir = DDGIGetProbeRayDir(rayIndex, RAYS_PER_PROBE, gridIdx, frameIndex);
            RayData rayData = Elysia_DDGI_LoadRayData(probeIndex * RAYS_PER_PROBE + rayIndex);

            // 方向越接近，权重越高
            float weight = max(0.f, dot(probeDirection, rayDir));

            float distWeight = pow(weight, 50.0f);
            float absDist = min(abs(rayData.Distance), probeMaxRayDistance);
            accumulatedDist += float2(absDist * distWeight, (absDist * absDist) * distWeight);
            distSumWeight += distWeight;

            // Backface hit, don't blend
            if (rayData.Distance < 0.0f)
            {
                continue;
            }
            float3 radiance = rayData.Radiance;
            // (Radiance * w, w)
            accumulatedIrradiance += float4(radiance * weight, weight);
        }
        float epsilon = float(RAYS_PER_PROBE - RELOCATE_RAY_COUNT) * 1e-9f;

        // NVIDIA 建议除以 (2.0 * sumWeight) 以匹配漫反射积分
        float3 netIrradiance = accumulatedIrradiance.rgb /
                               (2.0f * max(accumulatedIrradiance.a, epsilon));
        netIrradiance = pow(netIrradiance, 1.0f / g_DDGIEncodingGamma);
        float2 netDist = accumulatedDist / max(distSumWeight, epsilon);

        float4 historyIrradiance = Elysia_DDGI_LoadIrradiance(id.xy);
        float2 historyDist = Elysia_DDGI_LoadDist(id.xy);
        float hysteresis = saturate(g_DDGIBlendWeight); // 历史权重
        float distHysteresis = hysteresis;
        float3 delta = netIrradiance - historyIrradiance;

        if (dot(historyIrradiance, historyIrradiance) == 0)
        {
            hysteresis = distHysteresis = 0.0f;
        }

        float significantChangeThreshold = 0.25f;
        float newDistributionChangeThreshold = 0.8f;

        float changeMagnitude = DDGIMaxComponent(abs(delta));
        if (changeMagnitude > significantChangeThreshold)
        {
            hysteresis = max(0, hysteresis - 0.15f);
        }
        if (changeMagnitude > newDistributionChangeThreshold)
        {
            hysteresis = 0.f;
        }

        // if (DDGIMaxComponent(historyIrradiance - netIrradiance) > g_ProbeIrradianceThreshold)
        // {
        //     hysteresis = max(0, hysteresis - 0.75f);
        // }
        // 亮度增幅限制 (Brightness Thresholding)
        // 防止由于 Ray Tracing 噪声产生的极亮像素导致探针闪烁
        float luminanceDelta = DDGILinearRGBToLuminance(delta);
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
        float2 finalDist = lerp(distDelta, historyDist, distHysteresis);
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