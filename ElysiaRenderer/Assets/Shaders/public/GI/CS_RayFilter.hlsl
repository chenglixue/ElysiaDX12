#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

struct DispatchArguments
{
    uint ThreadGroupCountX;
    uint ThreadGroupCountY;
    uint ThreadGroupCountZ;
};

struct CompactedRay
{
    float4 Position;
    float4 Data;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SourceRayDataBufferIndex;
    UINT g_CompactedRayBufferIndex;
    UINT g_CompactedIndicesBufferIndex;
    UINT g_GlobalCounterBufferIndex;
    UINT g_IndirectArgsBufferIndex;
}

[numthreads(64, 1, 1)]
void RayCompaction(uint3 id : SV_DispatchThreadID)
{
    uint totalRays = PROBE_COUNT * RAYS_PER_PROBE;
    [branch]
    if (id.x >= totalRays)
        return;

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
        RWStructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];

        // curr thread compute global address offset
        InterlockedAdd(globalCounter[0], waveActiveCount, waveBaseOffset);
    }

    // Distribute acquired global address offset to other Wave threads
    waveBaseOffset = WaveReadLaneFirst(waveBaseOffset);

    if (isValid)
    {
        // Compute current thread's offset in the Wave
        uint localOffset = WavePrefixCountBits(isValid);
        uint writeIndex = waveBaseOffset + localOffset;

        CompactedRay compactedRay = (CompactedRay)0;
        compactedRay.Position = rayData.Position;
        compactedRay.Data = rayData.Data;

        RWStructuredBuffer<CompactedRay> compactedRayBuffer = ResourceDescriptorHeap[g_CompactedRayBufferIndex];
        RWStructuredBuffer<UINT> compactedIndicesBuffer = ResourceDescriptorHeap[g_CompactedIndicesBufferIndex];
        compactedRayBuffer[writeIndex] = compactedRay;
        compactedIndicesBuffer[writeIndex] = id.x;
    }
}

[numthreads(1, 1, 1)]
void CalcIndirectArgsCS()
{
    RWStructuredBuffer<uint> globalCounter = ResourceDescriptorHeap[g_GlobalCounterBufferIndex];
    RWStructuredBuffer<DispatchArguments> compactedIndicesBuffer = ResourceDescriptorHeap[g_IndirectArgsBufferIndex];

    uint totalValidRays = globalCounter[0];
    compactedIndicesBuffer[0].ThreadGroupCountX = (totalValidRays + 63) / 64;
    compactedIndicesBuffer[0].ThreadGroupCountY = 1;
    compactedIndicesBuffer[0].ThreadGroupCountZ = 1;
}