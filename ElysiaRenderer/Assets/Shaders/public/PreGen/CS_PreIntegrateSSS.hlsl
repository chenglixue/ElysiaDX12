#include "private\Common.hlsl"

#define GROUP_SIZE 8
cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_TargetSize;

    UINT g_PreIntegrateSSSLUT;
}

void Elysia_Save_PreIntegrateSSS(UINT2 writePos, float3 saveValue)
{
    RWTexture2D<float3> o = ResourceDescriptorHeap[g_PreIntegrateSSSLUT];
    o[writePos] = saveValue;
}

float3 Gaussian(float v, float r)
{
    return 1.0 / sqrt(2.0 * PI * v) * exp(-(r * r) / (2.0 * v));
}

float3 ApproximateScatter(float r)
{
    return float3(0.0, 0.0, 0.0)
           + Gaussian(0.0064, r) * float3(0.233, 0.455, 0.649)
           + Gaussian(0.0484, r) * float3(0.100, 0.336, 0.344)
           + Gaussian(0.187, r) * float3(0.118, 0.198, 0.0)
           + Gaussian(0.567, r) * float3(0.113, 0.007, 0.007)
           + Gaussian(1.99, r) * float3(0.358, 0.004, 0.0)
           + Gaussian(7.41, r) * float3(0.233, 0.0, 0.0);
}

float3 GetLUT(float2 uv)
{
    float NoL = uv.x;
    float oneOverR = uv.y;

    float theta = acos(NoL * 2.f - 1.f);
    float radius = 1.f / oneOverR;

    float3 totalWeight = 0.f, totalLight = 0.f;
    for (float x = -PI / 2; x <= PI / 2; x += PI * 0.001f)
    {
        float sampleDistance = abs(2.f * radius * sin(x / 2));
        float3 weight = ApproximateScatter(sampleDistance);

        totalWeight += weight;
        totalLight += saturate(cos(theta + x)) * weight;
    }

    float3 result = totalLight / totalWeight;

    return result;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void PreIntegrateSSS(uint3 globalID : SV_DispatchThreadID)
{
    uint2 readPos = globalID.xy;
    uint2 writePos = globalID.xy;
    if (writePos.x >= g_TargetSize.x || writePos.y >= g_TargetSize.y)
        return;

    float2 screenUV = (float2(readPos) + 0.5f) * g_TargetSize.zw;
    float3 scatterDiffuse = GetLUT(screenUV);

    Elysia_Save_PreIntegrateSSS(writePos, scatterDiffuse);
}