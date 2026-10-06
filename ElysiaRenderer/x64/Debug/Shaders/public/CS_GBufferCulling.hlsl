#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewProjMatrix;

    Vector4 g_HIZTexSize;
    Vector4 g_FrustumMaxPoint;
    Vector4 g_FrustumMinPoint;

    UINT g_TotalObjectCount;
    UINT g_AABBInstanceDatasIndex;
    UINT g_VisbibleCounterBufferIndex;
    UINT g_VisbibleIndexBufferIndex;

    UINT g_HIZTexIndex;
    UINT g_HIZMipmapCount;
    bool g_EnableHIZ;
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
bool IsAABBOutsideFrustum(float3 AABBCenter, float3 AABBExtent, float4 FrustumPlanes[6]);
bool IsInClipSpace(float4 clipSpacePosition);
bool ProjectAABB(float3 AABBCenter,
                 float3 AABBExtent,
                 float4x4 viewProj,
                 out float2 screenMin,
                 out float2 screenMax,
                 out float closestZ);
uint4 GetHZBTexels(float2 minUV, float2 maxUV, float mipLevel);
float GetFurthestDepth4x4(float2 minUV, float2 maxUV, float mipLevel);
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
float LoadHIZ(float2 sampleUV, float mipmap)
{
    float HIZ = SampleTexture2D_LOD(g_HIZTexIndex, sampleUV, ClampPointSampler, mipmap);
    return HIZ;
}

[numthreads(1, 1, 1)]
void ClearCounterBuffer(uint3 GlobalID : SV_DispatchThreadID)
{
    RWStructuredBuffer<int> counterBuffer = ResourceDescriptorHeap[g_VisbibleCounterBufferIndex];
    counterBuffer[0] = 0;
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

    float3 AABBExtent = (AABBData.Max - AABBData.Min) * 0.5f;
    float3 AABBCenter = (AABBData.Max + AABBData.Min) * 0.5f;
    // bool isCull = FrustumAABBCull(AABBCenter, AABBExtent, g_FrustumPlanes);
    // if (isCull)
    // {
    //     return;
    // }

    if (g_EnableHIZ)
    {
        float2 minUV, maxUV;

        float cloestZ;

        if (ProjectAABB(AABBCenter, AABBExtent, viewProjMatrix, minUV, maxUV, cloestZ))
        {
            float4 uvRect = float4(minUV, maxUV);
            float2 viewSize = (maxUV - minUV) * g_HIZTexSize.xy;
            float maxPixelSize = max(viewSize.x, viewSize.y);

            float mipLevel = log2(maxPixelSize);
            if (mipLevel > g_HIZMipmapCount - 0.5f)
                return;
            mipLevel = clamp(ceil(mipLevel) - 4.3f, 0.0f, (float)g_HIZMipmapCount - 1.0f);

            float depthLT = SampleTexture2D_LOD(g_HIZTexIndex, uvRect.xy, ClampPointSampler, mipLevel);
            float depthRT = SampleTexture2D_LOD(g_HIZTexIndex, uvRect.zw, ClampPointSampler, mipLevel);
            float depthLB = SampleTexture2D_LOD(g_HIZTexIndex, uvRect.zy, ClampPointSampler, mipLevel);
            float depthRB = SampleTexture2D_LOD(g_HIZTexIndex, uvRect.xw, ClampPointSampler, mipLevel);
            float farZ = max(depthRB, max(depthLB, max(depthLT, depthRT)));

            if (farZ + 1e-5f < cloestZ)
            {
                return;
            }

        }
    }

    int writeIndex = 0;
    CounterAdd(writeIndex);
    SaveVisibileIndexBuffer(writeIndex, readIndex);
}

bool IsOutSidePlane(float4 plane, float3 pos)
{
    if (dot(plane.xyz, pos) + plane.w > 0)
        return true;
    return false;
}
bool FrustumAABBCull(float3 AABBCenter, float3 AABBExtent, float4 FrustumPlanes[6])
{
    float3 corners[8];

    float3 C = AABBCenter;
    float3 E = AABBExtent;
    corners[0] = C + float3(-E.x, -E.y, -E.z);
    corners[1] = C + float3(E.x, -E.y, -E.z);
    corners[2] = C + float3(E.x, -E.y, E.z);
    corners[3] = C + float3(-E.x, -E.y, E.z);

    corners[4] = C + float3(-E.x, E.y, -E.z);
    corners[5] = C + float3(E.x, E.y, -E.z);
    corners[6] = C + float3(E.x, E.y, E.z);
    corners[7] = C + float3(-E.x, E.y, E.z);

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float4 plane = FrustumPlanes[i];
        float3 normal = plane.xyz;
        int outCount = 0;
        for (int j = 0; j < 8; ++j)
        {
            float3 AABBPos = corners[j];
            if (IsOutSidePlane(plane, AABBPos))
            {
                outCount ++;
            }
            if (outCount == 8)
            {
                return true;
            }
        }

        // float projectionRadius = dot(AABBExtent, abs(normal));
        // float distance = dot(AABBCenter, normal) + plane.w;
        //
        // if (distance > projectionRadius)
        // {
        //     return true;
        // }
    }
    return false;
}
bool IsInClipSpace(float4 clipSpacePosition)
{
    return clipSpacePosition.x > -clipSpacePosition.w && clipSpacePosition.x < clipSpacePosition.w &&
           clipSpacePosition.y > -clipSpacePosition.w && clipSpacePosition.y < clipSpacePosition.w &&
           clipSpacePosition.z > 0 && clipSpacePosition.z < clipSpacePosition.w;
}

bool IsAABBOutsideFrustum(float3 AABBCenter, float3 AABBExtent, float4 FrustumPlanes[6])
{
    float3 minPos = AABBCenter - AABBExtent;
    float3 maxPos = AABBCenter + AABBExtent;

    float3 corners[8] = {
        float3(minPos.x, minPos.y, minPos.z),
        float3(minPos.x, minPos.y, maxPos.z),
        float3(minPos.x, maxPos.y, minPos.z),
        float3(minPos.x, maxPos.y, maxPos.z),
        float3(maxPos.x, minPos.y, minPos.z),
        float3(maxPos.x, minPos.y, maxPos.z),
        float3(maxPos.x, maxPos.y, minPos.z),
        float3(maxPos.x, maxPos.y, maxPos.z)
    };

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float3 n = FrustumPlanes[i].xyz;
        float d = FrustumPlanes[i].w;

        bool allOutside = true;
        [unroll]
        for (int j = 0; j < 8; ++j)
        {
            float dist = dot(n, corners[j]) + d;
            if (dist <= 0.0f)
            {
                allOutside = false;
                break;
            }
        }
        if (allOutside)
            return true;
    }
    return false;
}

bool ProjectAABB(float3 AABBCenter,
                 float3 AABBExtent,
                 float4x4 viewProj,
                 out float2 screenMin,
                 out float2 screenMax,
                 out float closestZ)
{
    float3 offsets[8] = {
        float3(1, 1, 1), float3(-1, 1, 1), float3(1, -1, 1), float3(-1, -1, 1),
        float3(1, 1, -1), float3(-1, 1, -1), float3(1, -1, -1), float3(-1, -1, -1)
    };

    screenMin = 1.0f;
    screenMax = 0.0f;
    closestZ = 1.0f;
    UINT inSideCount = 0;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float3 positionWS = AABBCenter + offsets[i] * AABBExtent;
        float4 clipPos = mul(float4(positionWS, 1.0f), viewProj);
        if (clipPos.w > 0.001f)
        {
            clipPos.xyz /= clipPos.w;
            float2 uv = clipPos.xy * float2(0.5f, -0.5f) + 0.5f;
            screenMin = min(screenMin, uv);
            screenMax = max(screenMax, uv);

            closestZ = min(closestZ, clipPos.z);
            inSideCount ++;
        }
    }

    if (inSideCount < 8)
    {
        return false;
    }

    return true;
}

uint4 GetHZBTexels(float2 minUV, float2 maxUV, float mipLevel)
{
    float2 mipSize = floor(g_HIZTexSize.xy / exp2(mipLevel));

    uint4 texels;
    texels.xy = uint2(max(0.0f, floor(minUV * mipSize)));
    texels.zw = uint2(min(mipSize - 1.0f, floor(maxUV * mipSize)));
    return texels;
}
float GetFurthestDepth4x4(float2 minUV, float2 maxUV, float mipLevel)
{
    uint4 texels = GetHZBTexels(minUV, maxUV, mipLevel);

    float2 texelSize = exp2(mipLevel) * g_HIZTexSize.zw;

    float2 baseUV = (float2(texels.xy) + 1.0f) * texelSize;

    float4 d00 = GatherRedTexture2D(g_HIZTexIndex, baseUV, ClampPointSampler, mipLevel);
    float4 d10 = GatherRedTexture2D(g_HIZTexIndex, baseUV + float2(2.0f * texelSize.x, 0), ClampPointSampler, mipLevel);
    float4 d01 = GatherRedTexture2D(g_HIZTexIndex, baseUV + float2(0, 2.0f * texelSize.y), ClampPointSampler, mipLevel);
    float4 d11 = GatherRedTexture2D(g_HIZTexIndex, baseUV + 2.0f * texelSize, ClampPointSampler, mipLevel);

    if (texels.x == texels.z)
    {
        d00.yz = 0.0f;
        d10 = 0.0f;
        d01.yz = 0.0f;
        d11 = 0.0f;
    }
    if (texels.y == texels.w)
    {
        d00.xy = 0.0f;
        d01 = 0.0f;
        d10.xy = 0.0f;
        d11 = 0.0f;
    }

    float4 max4;
    max4.x = max(max(d00.x, d00.y), max(d00.z, d00.w));
    max4.y = max(max(d10.x, d10.y), max(d10.z, d10.w));
    max4.z = max(max(d01.x, d01.y), max(d01.z, d01.w));
    max4.w = max(max(d11.x, d11.y), max(d11.z, d11.w));

    return max(max(max4.x, max4.y), max(max4.z, max4.w));
}