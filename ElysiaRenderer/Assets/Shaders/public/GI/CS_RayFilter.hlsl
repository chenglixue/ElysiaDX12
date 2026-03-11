#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

struct DispatchArguments
{
    uint ThreadGroupCountX;
    uint ThreadGroupCountY;
    uint ThreadGroupCountZ;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SourceRayDataBufferIndex;
    UINT g_CompactedRayBufferIndex;
    UINT g_CompactedIndicesBufferIndex;
    UINT g_GlobalCounterBufferIndex;
    UINT g_IndirectArgsBufferIndex;
    UINT g_ProbeStatesIndex;
    UINT g_GIDataBufferIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_RelocationLUTIndex;

    float4 g_GridOrigin;
    float4 g_GridSpacing;
    float4 g_GridDimensions;
}

[numthreads(1, 1, 1)]
void ResetCounter(uint3 id : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];
    globalCounter[0] = 0;
}

[numthreads(64, 1, 1)]
void RayCompaction(uint3 id : SV_DispatchThreadID)
{
    uint totalRays = PROBE_COUNT * RAYS_PER_PROBE;
    [branch]
    if (id.x >= totalRays)
        return;

    UINT probeIndex = id.x / RAYS_PER_PROBE;
    UINT rayIndex = id.x % RAYS_PER_PROBE;

    UINT2 probeOffsetIndexID = UINT2(probeIndex % 64, probeIndex / 64);
    Texture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
    UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
    StructuredBuffer<float4> relocationLUT = ResourceDescriptorHeap[g_RelocationLUTIndex];
    float3 probeOffset = relocationLUT[index];
    float3 probePosWS = GetProbeWorldPosition(probeIndex, g_GridOrigin, g_GridSpacing, g_GridDimensions) + probeOffset;
    float distToCamera = distance(probePosWS, cameraPosWS);

    uint updateInterval = 64;
    if (distToCamera < NEAR_GI_DISTANCE)
    {
        updateInterval = 4;
    }
    else if (distToCamera < MIDDLE_GI_DISTANCE)
    {
        updateInterval = 16;
    }
    [branch]
    if (probeIndex % updateInterval != frameIndex % updateInterval)
        return;

    StructuredBuffer<UINT> ProbeStatesBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
    uint probeState = ProbeStatesBuffer[probeIndex];
    [branch]
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RELOCATE_RAY_COUNT)
    {
        return;
    }

    RWStructuredBuffer<CompactedRay> compactedRayBuffer = ResourceDescriptorHeap[g_CompactedRayBufferIndex];
    RWStructuredBuffer<UINT> compactedIndicesBuffer = ResourceDescriptorHeap[g_CompactedIndicesBufferIndex];
    RWStructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];
    StructuredBuffer<RayData> RayDataBuffer = ResourceDescriptorHeap[g_SourceRayDataBufferIndex];
    RayData rayData = RayDataBuffer[id.x];
    float distance = rayData.Position.w;
    bool isValid = distance >= 0.f && distance < DXR_MAX;

    // curr wave valid ray count
    UINT waveActiveCount = WaveActiveCountBits(isValid);

    uint waveBaseOffset = 0;
    // curr wave has non-waiting threads
    if (WaveIsFirstLane())
    {
        // curr thread compute global address offset
        InterlockedAdd(globalCounter[0], waveActiveCount, waveBaseOffset);
    }

    // Distribute acquired global address offset to other Wave threads
    waveBaseOffset = WaveReadLaneFirst(waveBaseOffset);

    if (isValid)
    {
        // Compute current thread's offset in the Wave(need valid ray, not nonValid)
        uint localOffset = WavePrefixCountBits(isValid);
        uint writeIndex = waveBaseOffset + localOffset;

        CompactedRay compactedRay;
        compactedRay.Position = rayData.Position;
        compactedRay.Data = rayData.Data;

        compactedRayBuffer[writeIndex] = compactedRay;
        compactedIndicesBuffer[writeIndex] = id.x;
    }
}

[numthreads(1, 1, 1)]
void CalcIndirectArgs()
{
    StructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];
    RWStructuredBuffer<DispatchArguments> compactedIndicesBuffer = ResourceDescriptorHeap[g_IndirectArgsBufferIndex];

    uint totalValidRays = globalCounter[0];
    compactedIndicesBuffer[0].ThreadGroupCountX = (totalValidRays + 63) / 64;
    compactedIndicesBuffer[0].ThreadGroupCountY = 1;
    compactedIndicesBuffer[0].ThreadGroupCountZ = 1;
}