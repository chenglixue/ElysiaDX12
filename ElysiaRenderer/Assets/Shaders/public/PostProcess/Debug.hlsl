#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Enabled

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
    float g_RandomRotation;
    UINT g_RayDataBufferIndex;
    UINT g_ProbeOffsetsIndex;
    bool g_IsEnableGILine;
    float g_DebugLineScale;
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
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float3 positionWS : WORLD_POSITION;
    float3 probeCenterWS : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : COLOR;
    float3 normalWS : TEXCOORD2;
    uint instanceID : SV_InstanceID;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i, UINT vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PSInput o = (PSInput)0;

    switch (g_DebugMode)
    {
    case DEBUG_GI:
    {
        if (g_IsEnableGILine)
        {
            uint totalVerticesPerProbe = RAYS_PER_PROBE * 2;
            uint probeIdx = vertexID / totalVerticesPerProbe;
            uint rayInProbeIdx = (vertexID / 2) % RAYS_PER_PROBE;
            uint isEndPoint = vertexID % 2; // 0 为起点，1 为终点

            StructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[
                g_ProbeOffsetsIndex];
            float3 probeWorldPos = GetProbeWorldPosition(probeIdx,
                                                         g_GridOrigin,
                                                         g_GridSpacing,
                                                         g_GridDimensions) +
                                   probeOffsetBuffer[probeIdx];

            StructuredBuffer<RayData> rayDataBuffer = ResourceDescriptorHeap[g_RayDataBufferIndex];
            uint rayDataIdx = probeIdx * RAYS_PER_PROBE + rayInProbeIdx;
            RayData data = rayDataBuffer[rayDataIdx];
            float3 dir = SphericalFibonacci(rayInProbeIdx, RAYS_PER_PROBE, g_RandomRotation);

            float3 finalPos = probeWorldPos;
            float3 finalColor = data.Radiance;

            if (isEndPoint)
            {
                // 根据 Distance 收缩
                // 如果是背面碰撞 (Distance < 0)，我们可以特殊标记
                float actualDist = abs(data.Distance);
                finalPos += dir * actualDist * g_DebugLineScale;

                // 如果撞击背面，将线段末端显示为红色或暗灰色以方便 Debug
                if (data.Distance < 0.0f)
                {
                    finalColor = float3(1.0f, 0.0f, 0.0f);
                }
            }
            else
            {
                // 起点颜色可以稍微暗一点，形成渐变感
                finalColor *= 0.2f;
            }
            o.positionCS = mul(float4(finalPos, 1.0f), viewProjMatrix);
            o.color.rgb = finalColor * 5;
            return o;
        }
        else
        {
            o.instanceID = instanceID;
            StructuredBuffer<float3> probeOffsetBuffer = ResourceDescriptorHeap[
                g_ProbeOffsetsIndex];
            o.probeCenterWS = GetProbeWorldPosition(instanceID,
                                                    g_GridOrigin,
                                                    g_GridSpacing,
                                                    g_GridDimensions) + probeOffsetBuffer[
                                  instanceID];

            o.positionWS = i.positionOS * g_ProbeRadius + o.probeCenterWS;

            o.positionCS = mul(float4(o.positionWS, 1.f), viewProjMatrix);

            float3 normalOS = i.positionOS;
            o.normalWS = normalize(normalOS);
        }

        break;
    }
    case DEBUG_AABB:
    {
        StructuredBuffer<AABBInstanceData> AABBDatas = ResourceDescriptorHeap[
            g_AABBInstanceDatasIndex];
        AABBInstanceData AABBData = AABBDatas[instanceID];
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
        if (g_IsEnableGILine)
        {
            result.rgb = i.color;
        }
        else
        {
            float3 N = normalize(i.normalWS); // 这里的 N 决定了当前像素“看向”哪个方向
            float3 finalRadiance = 0.0f;
            float totalWeight = 0.0f;

            for (uint r = 0; r < RAYS_PER_PROBE; r ++)
            {
                // 1. 恢复该射线的发射方向
                float3 rayDir = SphericalFibonacci(r, RAYS_PER_PROBE, g_RandomRotation);

                // 2. 计算权重：使用高次幂（如 16 或 32）来获取清晰的细节
                float weight = max(0.0f, dot(N, rayDir));
                weight = pow(weight, 16.0f);

                // 3. 加权累加辐射度
                RayData rayData = Elysia_DDGI_LoadRayData(i.instanceID * RAYS_PER_PROBE + r);

                // 排除 Miss 的射线（Distance=10000），防止球体变黑
                if (rayData.Distance < 10000.0f)
                {
                    finalRadiance += rayData.Radiance * weight;
                    totalWeight += weight;
                }
            }

            // 4. 归一化并输出颜色
            float3 color = (totalWeight > 0.0f)
                               ? (finalRadiance / totalWeight)
                               : float3(0.1f, 0.1f, 0.1f);
            result.rgb = color;
        }

        o.target0 = result;
        break;
    }
    }

    o.target0.a = 1;
    return o;
}