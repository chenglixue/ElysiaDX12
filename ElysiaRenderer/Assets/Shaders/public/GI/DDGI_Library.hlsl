#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#define GROUP_SIZE 8
#define IRRADIANCE_GROUP_SIZE 8
#define DISTANCE_GROUP_SIZE 8
#define PROBE_COUNT 1024
#define Rays_Per_Probe 32

cbuffer PassConstant : register(b0, space2)
{
    float4 g_GridSpacing;
    float4 g_GridOrigin;
    float4 g_GridDimensions;

    uint g_RayDataBufferIndex;
    float g_RandomRotation;
}


// void Elysia_DDGI_StoreIrradiance(uint2 id, float3 val)
// {
//     RWTexture2D<float4> o = ResourceDescriptorHeap[g_IrradianceTexIndex];
//     o[id].rgb = val;
// }

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

float3 SphericalFibonacci(uint sampleIndex, uint numSamples, float rotation);

[shader("raygeneration")]
void GenerateRayMain()
{
    uint probeIndex = DispatchRaysIndex().x;
    uint rayIndex = DispatchRaysIndex().y;

    float3 rayDir = SphericalFibonacci(rayIndex, Rays_Per_Probe, g_RandomRotation);

    float3 fakeRadiance = rayDir * 0.5f + 0.5f;
    float fakeDistance = 10.0f;

    uint writeIndex = probeIndex * Rays_Per_Probe + rayIndex;
    Elysia_DDGI_StoreRayData(writeIndex, fakeRadiance, fakeDistance);
}

float3 SphericalFibonacci(uint sampleIndex, uint numSamples, float rotation)
{
    float b = (sqrt(5.0) * 0.5 + 0.5) - 1.0;
    float phi = 2.0 * 3.1415926f * b;

    float theta = phi * sampleIndex + rotation;
    float cosPhi = 1.0 - (float(sampleIndex) + 0.5) / float(numSamples) * 2.0;
    float sinPhi = sqrt(saturate(1.0 - cosPhi * cosPhi));

    return float3(cos(theta) * sinPhi, cosPhi, sin(theta) * sinPhi);
}