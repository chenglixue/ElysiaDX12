#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_GridOrigin;
    float4 g_GridSpacing;
    float4 g_GridDimensions;
    float4 g_RandomRotation;

    UINT g_ProbeOffsetsIndex;
    UINT g_ProbeStatesIndex;
    UINT g_StaticAABBIndex;
    UINT g_RayDataBufferIndex;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_RelocationLUTIndex;
    UINT g_GIDataBufferIndex;
    float g_DDGIBlendWeight;
    float g_ProbeIrradianceThreshold;
    float g_ProbeBrightnessThreshold;
    float g_DDGIEncodingGamma;
    UINT g_StaticAABBCount;
}

static const float PROBE_BACKFACE_THRESHOLD = 0.25f;    // % 射线撞背面视为在内部
static const float PROBE_MIN_FRONTFACE_DISTANCE = 0.3f; // % 射线撞背面视为在内部
static const float PROBE_MAX_OFFSET_FRACTION = 0.45f;   // 最大允许偏移量 (相对于Grid间距的比例, 0.5是边界, 0.45是安全区)

groupshared float3 g_RayDirection[RAYS_PER_PROBE];
groupshared float3 g_RayRadiance[RAYS_PER_PROBE];
groupshared float g_RayDistance[RAYS_PER_PROBE];

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

void DDGI_Store_Probe_Offset_Index(UINT2 id, uint value)
{
    RWTexture2D<UINT> probeOffsetTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    probeOffsetTex[id] = value;
}
UINT DDGI_Load_Probe_Offset_Index(UINT2 id)
{
    RWTexture2D<UINT> probeOffsetTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    return probeOffsetTex[id];
}

[numthreads(64, 1, 1)]
void ResetProbeStates(uint3 id : SV_DispatchThreadID)
{
    Elysia_DDGI_StoreProbeState(id.x, PROBE_STATE_ACTIVE);
}

[numthreads(64, 1, 1)]
void UpdateProbeStates(uint3 id : SV_DispatchThreadID)
{
    uint probeIndex = id.x;
    [branch]
    if (probeIndex >= PROBE_COUNT)
        return;

    int backfaceCount = 0;
    float hitDistances[RELOCATE_RAY_COUNT];
    StructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
    RWStructuredBuffer<UINT> probeStateBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];

    for (int rayIndex = 0; rayIndex < RELOCATE_RAY_COUNT; rayIndex ++)
    {
        // Get the coordinates for the probe ray in the RayData texture array
        GIData giData = GIDataBuffer[probeIndex * RAYS_PER_PROBE + rayIndex];

        // Load the hit distance for the ray
        hitDistances[rayIndex] = giData.Distance;

        // Increment the count if a backface is hit
        backfaceCount += (hitDistances[rayIndex] < 0.f);
    }
    if ((float)backfaceCount * rcp((float)RELOCATE_RAY_COUNT) > PROBE_BACKFACE_THRESHOLD)
    {
        probeStateBuffer[probeIndex] = PROBE_STATE_INACTIVE;
        return;
    }

    // UINT2 probeOffsetIndexID = UINT2(probeIndex % 64, probeIndex / 64);
    // Texture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    // UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
    // StructuredBuffer<float4> relocationLUT = ResourceDescriptorHeap[g_RelocationLUTIndex];
    // float3 probeOffset = relocationLUT[index];
    float3 probeOffset = probeOffsetBuffer[probeIndex];

    float3 probePosWS = GetProbeWorldPosition(probeIndex, g_GridOrigin, g_GridSpacing, g_GridDimensions) + probeOffset;
    float probeSpacingMax = max(g_GridSpacing.x, max(g_GridSpacing.y, g_GridSpacing.z));

    bool isNearStatic = false;
    for (UINT i = 0; i < g_StaticAABBCount; ++i)
    {
        AABBData AABB = Elysia_DDGI_LoadeStaticAABB(i);
        [branch]
        if (IsPointInAABB(probePosWS, AABB, probeSpacingMax))
        {
            isNearStatic = true;
            break;
        }
    }

    [branch]
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

[numthreads(64, 1, 1)]
void ClearProbeOffsetBuffer(uint3 id : SV_DispatchThreadID)
{
    uint probeIndex = id.x;
    if (probeIndex >= PROBE_COUNT)
        return;
    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    probeOffsetBuffer[probeIndex] = 0.f;
}

[numthreads(64, 1, 1)]
void RelocateProbes(UINT3 id : SV_DispatchThreadID)
{
    UINT probeIndex = id.x;
    [branch]
    if (probeIndex >= PROBE_COUNT)
        return;

    UINT3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);

    RWStructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[g_ProbeOffsetsIndex];
    StructuredBuffer<GIData> giDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
    // RWStructuredBuffer<UINT> probeStateBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
    // StructuredBuffer<float4> relocationLUT = ResourceDescriptorHeap[g_RelocationLUTIndex];

    uint2 texCoord = uint2(probeIndex % 64, probeIndex * rcp(64));
    UINT currIndex = DDGI_Load_Probe_Offset_Index(texCoord);

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
        GIData giData = giDataBuffer[rayIndex];
        float hitDistance = giData.Distance;

        if (hitDistance < 0.f)
        {
            backFaceCount ++;
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
    float3 fullOffset = float3(1e27f, 1e27f, 1e27f);

    // If there’s a close backface AND you see more than 25% backfaces, assume you’re inside something.
    if (closestBackfaceIndex != -1 && (backFaceCount * rcp(RELOCATE_RAY_COUNT)) > PROBE_BACKFACE_THRESHOLD)
    {
        // direction to closest backface scaled by distance
        float3 backfaceDir = DDGIGetProbeRayDir(closestBackfaceIndex, RAYS_PER_PROBE, g_RandomRotation);
        fullOffset = currentOffset + (backfaceDir * (
                                          closestBackfaceDist + PROBE_MIN_FRONTFACE_DISTANCE * 0.5f));
    }
    else if (closestFrontfaceDist < PROBE_MIN_FRONTFACE_DISTANCE)
    {
        float3 closeDir = DDGIGetProbeRayDir(closestFrontfaceIndex, RAYS_PER_PROBE, g_RandomRotation);
        float3 farDir = DDGIGetProbeRayDir(farthestFrontfaceIndex, RAYS_PER_PROBE, g_RandomRotation);
        if (dot(closeDir, farDir) <= 0.f)
        {
            // 限制移动步长，确保不会穿过最远的可见平面
            farDir *= min(farthestFrontfaceDist, 1.f);
            fullOffset = currentOffset + farDir;
        }
    }
    else if (closestFrontfaceDist > PROBE_MIN_FRONTFACE_DISTANCE)
    {
        float moveBackMargin = min(closestFrontfaceDist - PROBE_MIN_FRONTFACE_DISTANCE, length(currentOffset));
        float3 moveBackDirection = normalize(-currentOffset);
        fullOffset = currentOffset + (moveBackMargin * moveBackDirection);
    }

    // 将offset匹配到最近的 LUT 索引
    // UINT bestIndex = 0;
    // float minError = 1e27f;
    // float3 normalizedOffset = fullOffset * rcp(max(g_GridSpacing * PROBE_MAX_OFFSET_FRACTION, 1e-6));
    // if (dot(normalizedOffset, normalizedOffset) < 0.2025f)
    // {
    //     // 在 128 个预设点中找一个离理想位置最近的
    //     for (uint i = 0; i < 32 * 4; ++i)
    //     {
    //         float d = distance(relocationLUT[i].xyz, fullOffset);
    //         if (d < minError)
    //         {
    //             minError = d;
    //             bestIndex = i;
    //         }
    //     }
    // }
    // else
    // {
    //     bestIndex = currIndex; // 超出范围则保持现状
    // }

    // DDGI_Store_Probe_Offset_Index(id, bestIndex);
    float normalizedOffset = fullOffset / g_GridSpacing;
    if (dot(normalizedOffset, normalizedOffset) < 0.2025f) // 0.45 * 0.45 == 0.2025
    {
        probeOffsetBuffer[probeIndex] = fullOffset;
    }
}

[numthreads(DDGI_PROBE_IRRADIANCE_NUM_TEXELS, DDGI_PROBE_IRRADIANCE_NUM_TEXELS, 1)]
void ProbeIrradianceBlending(uint3 id : SV_DispatchThreadID,
                             uint3 GroupThreadID : SV_GroupThreadID,
                             uint3 GroupID : SV_GroupID)
{
    const UINT N = DDGI_PROBE_IRRADIANCE_NUM_TEXELS;
    const UINT numThreads = N * N;
    const uint localIdx = GroupThreadID.y * N + GroupThreadID.x;
    const UINT probeIndex = GroupID.x + (GroupID.y * g_GridDimensions.x);

    [branch]
    if (probeIndex >= PROBE_COUNT || probeIndex < 0)
        return;

    const UINT currProbeState = Elysia_DDGI_LoadeProbeState(probeIndex);

    for (UINT i = localIdx; i < RAYS_PER_PROBE; i += numThreads)
    {
        UINT rayIndex = probeIndex * RAYS_PER_PROBE + i;
        RWStructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
        GIData giData = GIDataBuffer[rayIndex];
        g_RayRadiance[i] = giData.Irradiance;
        g_RayDirection[i] = DDGIGetProbeRayDir(i, RAYS_PER_PROBE, g_RandomRotation);
    }

    GroupMemoryBarrierWithGroupSync();

    bool isBorder = (GroupThreadID.x == 0 || GroupThreadID.x == (N - 1) ||
                     GroupThreadID.y == 0 || GroupThreadID.y == (N - 1));
    [branch]
    if (!isBorder)
    {
        if (currProbeState == PROBE_STATE_INACTIVE)
            return;
        float2 uv = (float2(GroupThreadID.xy) - 1.f + 0.5f) * rcp((float)(N - 2));
        float2 octUV = uv * 2.0f - 1.0f;
        float3 probeDirection = OctDecode(octUV);

        float probeRandomRayBackfaceThreshold = 0.1f;
        uint backfaces = 0;
        uint maxBackfaces = uint((RAYS_PER_PROBE - RELOCATE_RAY_COUNT) * probeRandomRayBackfaceThreshold);
        float4 accumulatedIrradiance = 0.0f;
        for (int rayIndex = RELOCATE_RAY_COUNT; rayIndex < RAYS_PER_PROBE; rayIndex ++)
        {
            float3 rayDir = g_RayDirection[rayIndex];
            float rayDistance = g_RayDistance[rayIndex];

            // Backface hit, don't blend
            [branch]
            if (rayDistance < 0.0f)
            {
                backfaces ++;
                if (backfaces >= maxBackfaces)
                    return;
                continue;
            }

            float3 rayRadiance = g_RayRadiance[rayIndex];
            // (Radiance * w, w)
            // 方向越接近，权重越高
            float weight = max(0.f, dot(probeDirection, rayDir));
            accumulatedIrradiance += float4(rayRadiance * weight, weight);
        }
        float epsilon = float(RAYS_PER_PROBE - RELOCATE_RAY_COUNT) * 1e-9f;
        float hysteresis = saturate(g_DDGIBlendWeight); // 历史权重

        float3 netIrradiance = accumulatedIrradiance.rgb / (2.0f * max(accumulatedIrradiance.a, epsilon));
        float4 historyIrradiance = Elysia_DDGI_LoadIrradiance(id.xy);

        if (dot(historyIrradiance, historyIrradiance) == 0)
        {
            hysteresis = 0.0f;
        }

        netIrradiance = pow(netIrradiance, 1.0f / g_DDGIEncodingGamma);
        float3 delta = netIrradiance - historyIrradiance;

        // float significantChangeThreshold = 0.25f;
        // float newDistributionChangeThreshold = 0.8f;
        // float changeMagnitude = DDGIMaxComponent(abs(delta));
        // if (changeMagnitude > significantChangeThreshold)
        // {
        //     hysteresis = max(0, hysteresis - 0.15f);
        // }
        // if (changeMagnitude > newDistributionChangeThreshold)
        // {
        //     hysteresis = 0.f;
        // }

        if (DDGIMaxComponent(historyIrradiance - netIrradiance) > g_ProbeIrradianceThreshold)
        {
            hysteresis = max(0, hysteresis - 0.75f);
        }
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

        float3 finalColor = historyIrradiance + lerpDelta;
        Elysia_DDGI_StoreIrradiance(id.xy, finalColor);
    }

    AllMemoryBarrierWithGroupSync();

    [branch]
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

        // 缝合 Irradiance
        float3 borderIrr = Elysia_DDGI_LoadIrradiance(globalCopyPos).rgb;

        Elysia_DDGI_StoreIrradiance(id.xy, borderIrr);
    }
}

[numthreads(DDGI_PROBE_DEPTH_NUM_TEXELS, DDGI_PROBE_DEPTH_NUM_TEXELS, 1)]
void ProbeDepthBlending(uint3 id : SV_DispatchThreadID,
                        uint3 GroupThreadID : SV_GroupThreadID,
                        uint3 GroupID : SV_GroupID)
{
    const UINT N = DDGI_PROBE_DEPTH_NUM_TEXELS;
    const UINT numThreads = N * N;
    const uint localIdx = GroupThreadID.y * N + GroupThreadID.x;
    const UINT probeIndex = GroupID.x + (GroupID.y * g_GridDimensions.x);

    [branch]
    if (probeIndex >= PROBE_COUNT)
        return;

    const UINT currProbeState = Elysia_DDGI_LoadeProbeState(probeIndex);

    for (UINT i = localIdx; i < RAYS_PER_PROBE; i += numThreads)
    {
        StructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
        GIData giData = GIDataBuffer[probeIndex * RAYS_PER_PROBE + i];
        g_RayDistance[i] = giData.Distance;
        g_RayDirection[i] = DDGIGetProbeRayDir(i, RAYS_PER_PROBE, g_RandomRotation);
    }

    GroupMemoryBarrierWithGroupSync();

    // uint3 gridIdx = GetProbeGridCoord(probeIndex, g_GridDimensions);
    // 将 [1, 6] 映射到八面体坐标的 [-1, 1]
    bool isBorder = (GroupThreadID.x == 0 || GroupThreadID.x == (N - 1) ||
                     GroupThreadID.y == 0 || GroupThreadID.y == (N - 1));
    [branch]
    if (!isBorder)
    {
        if (currProbeState == PROBE_STATE_INACTIVE)
            return;
        float2 uv = (float2(GroupThreadID.xy) - 1.f + 0.5f) / (float)(N - 2);
        float2 octUV = uv * 2.0f - 1.0f;
        float3 probeDirection = OctDecode(octUV);

        float2 accumulatedDist = 0.0f;
        float distSumWeight = 0.f;
        float probeMaxRayDistance = length(g_GridSpacing) * 1.5f;
        for (int rayIndex = RELOCATE_RAY_COUNT; rayIndex < RAYS_PER_PROBE; rayIndex ++)
        {
            float3 rayDir = g_RayDirection[rayIndex];
            float rayDistance = g_RayDistance[rayIndex];

            float weight = max(0.f, dot(probeDirection, rayDir));
            float distWeight = pow(weight, 50);
            float absDist = min(abs(rayDistance), probeMaxRayDistance);
            accumulatedDist += float2(absDist * distWeight, (absDist * absDist) * distWeight);
            distSumWeight += distWeight;
        }
        float epsilon = float(RAYS_PER_PROBE - RELOCATE_RAY_COUNT) * 1e-9f;
        float distHysteresis = saturate(g_DDGIBlendWeight);
        float2 historyDist = Elysia_DDGI_LoadDist(id.xy);

        float2 netDist = accumulatedDist / (2.f * max(distSumWeight, epsilon));

        if (dot(historyDist, historyDist) == 0)
        {
            distHysteresis = 0.0f;
        }

        float2 finalDist = lerp(netDist, historyDist, distHysteresis);
        Elysia_DDGI_StoreDist(id.xy, finalDist);
    }

    AllMemoryBarrierWithGroupSync();

    [branch]
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
        float2 borderDist = Elysia_DDGI_LoadDist(globalCopyPos).rg;

        Elysia_DDGI_StoreDist(id.xy, borderDist);
    }
}