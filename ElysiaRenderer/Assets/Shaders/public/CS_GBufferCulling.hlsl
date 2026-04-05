#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TotalObjectCount;
    UINT g_AABBInstanceDatasIndex;
    UINT g_VisbibleCounterBufferIndex;
    UINT g_VisbibleIndexBufferIndex;

    Vector4 g_FrustumPlanes[6];
}

// world
struct AABBInstanceData
{
    Vector3 Min;
    float pad0;

    Vector3 Max;
    float pad1;
};

bool FrustumAABBCull(float3 AABBCenter, float3 AABBExtents, Vector4 FrustumPlanes[6]);
void CounterAdd(out int writeIndex)
{
    RWStructuredBuffer<int> counterBuffer = ResourceDescriptorHeap[g_VisbibleCounterBufferIndex];
    InterlockedAdd(counterBuffer[0], 1, writeIndex);
}
void SaveVisibileIndexBuffer(int writeIndex, int saveValue)
{
    RWStructuredBuffer<int> VisbibleIndexBuffer = ResourceDescriptorHeap[g_VisbibleIndexBufferIndex];
    VisbibleIndexBuffer[writeIndex] = saveValue;
}

[numthreads(GROUP_SIZE, 1, 1)]
void Gbuffer_Culling(uint3 GlobalID : SV_DispatchThreadID)
{
    UINT readIndex = GlobalID.x;
    if (readIndex >= g_TotalObjectCount)
    {
        return;
    }

    StructuredBuffer<AABBInstanceData> AABBDatas = ResourceDescriptorHeap[g_AABBInstanceDatasIndex];
    AABBInstanceData AABBData = AABBDatas[readIndex];

    float3 AABBExtent = (AABBData.Max - AABBData.Min) * 0.5h;
    float3 AABBCenter = (AABBData.Max + AABBData.Min) * 0.5h;
    bool isCull = FrustumAABBCull(AABBCenter, AABBExtent, g_FrustumPlanes);
    if (isCull)
    {
        return;
    }

    int writeIndex = 0;
    CounterAdd(writeIndex);
    SaveVisibileIndexBuffer(writeIndex, GlobalID);
}

bool FrustumAABBCull(float3 AABBCenter, float3 AABBExtent, Vector4 FrustumPlanes[6])
{
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float4 plane = FrustumPlanes[i];

        float r = dot(AABBExtent, abs(plane));

        float d = dot(AABBCenter, plane.xyz) + plane.w;

        if (d >= -r)
        {
            return false;
        }
    }

    return true;
}