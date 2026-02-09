#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Enabled

#define PROBE_COUNT 1024
#define Rays_Per_Probe 32

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_IrradianceTexIndex;
    UINT g_AABBInstanceDatasIndex;
    UINT g_DebugMode;

    Vector4 screenSize;
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    Vector4 g_GridOrigin;
    Vector4 g_GridSpacing;
    Vector4 g_GridDimensions;
    float g_ProbeRadius;
    UINT g_RayDataBufferIndex;
}

struct AABBInstanceData
{
    Vector3 Min;
    float pad0;

    Vector3 Max;
    float pad1;

    Vector4 Color;
};

RayData Elysia_DDGI_LoadRayData(uint readIndex)
{
    RWStructuredBuffer<RayData> rayDatas = ResourceDescriptorHeap[g_RayDataBufferIndex];
    return rayDatas[readIndex];
}

#define DEBUG_NONE 0
#define DEBUG_AO 1
#define DEBUG_GI 2
#define DEBUG_NORMAL 3
#define DEBUG_AABB 4

struct VSInput
{
    float3 positionOS : POSITION;
    uint instanceID : SV_InstanceID;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float3 positionWS : WORLD_POSITION;
    float3 probeCenterWS : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint instanceID : SV_InstanceID;
    float4 color : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i, UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput)0;

    switch (g_DebugMode)
    {
    case DEBUG_GI:
    {
        o.instanceID = i.instanceID;
        o.probeCenterWS = GetProbeWorldPosition(i.instanceID,
                                                g_GridOrigin,
                                                g_GridSpacing,
                                                g_GridDimensions);

        o.positionWS = i.positionOS * g_ProbeRadius + o.probeCenterWS;

        o.positionCS = mul(float4(o.positionWS, 1.f), viewProjMatrix);
        break;
    }
    case DEBUG_AABB:
    {
        StructuredBuffer<AABBInstanceData> AABBDatas = ResourceDescriptorHeap[
            g_AABBInstanceDatasIndex];
        AABBInstanceData AABBData = AABBDatas[i.instanceID];
        float3 size = AABBData.Max - AABBData.Min;
        float3 center = (AABBData.Max + AABBData.Min) * 0.5f;

        o.positionWS = (i.positionOS * 0.5f) * size + center;
        o.positionCS = mul(float4(o.positionWS, 1.0f), viewProjMatrix);
        o.color = AABBData.Color;
        break;
    }
    case DEBUG_AO:
    case DEBUG_NORMAL:
        o.uv = float2((vertexID << 1) & 2, vertexID & 2);
        o.positionCS = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);

    }

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    switch (g_DebugMode)
    {
    case DEBUG_AO:
    {
        float AO = SampleTexture2D(g_AOIndex, i.uv, ClampLinearSampler);
        o.target0 = AO;
        break;
    }
    case DEBUG_NORMAL:
    {
        float3 normal = SampleNormalWS(i.uv);
        o.target0 = float4(normal, 1.f);
        break;
    }
    case DEBUG_AABB:
    {
        o.target0 = i.color;
        break;
    }
    case DEBUG_GI:
    {
        float4 result = 0.f;
        [unroll]
        for (int rayIndex = 0; rayIndex < Rays_Per_Probe; rayIndex ++)
        {
            uint rayDataIndex = i.instanceID * Rays_Per_Probe + rayIndex;
            RayData rayData = Elysia_DDGI_LoadRayData(rayDataIndex);
            result += float4(rayData.Radiance, rayData.Distance);
        }
        result /= Rays_Per_Probe;

        o.target0 = result;
        break;
    }
    }
    return o;
}