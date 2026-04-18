#include "private\ShadingCommon.hlsl"
#include "private\DDGICommon.hlsli"

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer NoCullNoMS
#pragma Blend Disabled
#pragma Depth Disabled

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_IrradianceTexIndex;
    UINT g_AABBInstanceDatasIndex;
    UINT g_DebugMode;

    Vector4 screenSize;
    Vector4 g_TargetSize;
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
    float4 g_RandomRotation;
    UINT g_RayDataBufferIndex;
    UINT g_GIDataBufferIndex;
    UINT g_ProbeOffsetsIndex;
    UINT g_ProbeStatesIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_ProbeRelocationLUTBufferIndex;
    bool g_IsEnableGILine;
    bool g_bHideInactiveProbe;
    float g_DebugLineScale;
    UINT g_TargetTexIndex;
    UINT g_MipmapLevel;
}

struct AABBInstanceData
{
    Vector3 Min;
    float pad0;

    Vector3 Max;
    float pad1;

    Vector4 Color;
};

GIData Elysia_DDGI_LoadRayData(uint readIndex)
{
    RWStructuredBuffer<GIData> GIDatas = ResourceDescriptorHeap[g_GIDataBufferIndex];
    return GIDatas[readIndex];
}

#define DEBUG_NONE 0
#define DEBUG_AO 1
#define DEBUG_GIPROBE 2
#define DEBUG_NORMAL 3
#define DEBUG_AABB 4
#define DEBUG_BLOOM 5
#define DEBUG_VELOCITY 6
#define DEBUG_GI 7
#define DEBUG_SHADOW_MASK 8
#define DEBUG_ALBEDO 9
#define DEBUG_EMISSION 10
#define DEBUG_METALLIC 11
#define DEBUG_ROUGHNESS 12

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
    case DEBUG_GIPROBE:
    {
        if (g_IsEnableGILine)
        {
            uint totalVerticesPerProbe = RAYS_PER_PROBE * 2;
            uint probeIdx = vertexID / totalVerticesPerProbe;
            uint rayInProbeIdx = (vertexID / 2) % RAYS_PER_PROBE;
            uint isEndPoint = vertexID % 2; // 0 为起点，1 为终点

            UINT2 probeOffsetIndexID = UINT2(probeIdx % 64, probeIdx / 64);
            RWTexture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
            UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
            StructuredBuffer<Vector4> ProbeRelocationLUTBuffer = ResourceDescriptorHeap[
                g_ProbeRelocationLUTBufferIndex];
            float3 probeOffset = ProbeRelocationLUTBuffer[index];
            float3 probeWorldPos = GetProbeWorldPosition(probeIdx,
                                                         g_GridOrigin,
                                                         g_GridSpacing,
                                                         g_GridDimensions) + probeOffset;
            // probeOffsetBuffer[probeIdx];

            StructuredBuffer<GIData> GIDataBuffer = ResourceDescriptorHeap[g_GIDataBufferIndex];
            uint rayDataIdx = probeIdx * RAYS_PER_PROBE + rayInProbeIdx;
            GIData data = GIDataBuffer[rayDataIdx];
            float3 dir = DDGIGetProbeRayDir(rayInProbeIdx, RAYS_PER_PROBE, g_RandomRotation);

            float3 finalPos = probeWorldPos;
            float3 finalColor = data.Irradiance;

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

            UINT probeIdx = instanceID;
            // UINT2 probeOffsetIndexID = UINT2(probeIdx % 64, probeIdx / 64);
            // RWTexture2D<uint> g_ProbeOffsetIndexTex = ResourceDescriptorHeap[g_ProbeOffsetIndexTexIndex];
            // UINT index = g_ProbeOffsetIndexTex.Load(UINT3(probeOffsetIndexID, 0));
            // StructuredBuffer<Vector4> ProbeRelocationLUTBuffer = ResourceDescriptorHeap[
            //     g_ProbeRelocationLUTBufferIndex];
            float3 probeOffset = probeOffsetBuffer[probeIdx];

            o.probeCenterWS = GetProbeWorldPosition(instanceID,
                                                    g_GridOrigin,
                                                    g_GridSpacing,
                                                    g_GridDimensions) + probeOffset;

            o.positionWS = i.positionOS * g_ProbeRadius + o.probeCenterWS;

            o.positionCS = mul(float4(o.positionWS, 1.f), viewProjMatrix);

            float3 normalOS = i.positionOS;
            o.normalWS = normalize(normalOS);

            if (g_bHideInactiveProbe)
            {
                StructuredBuffer<UINT> ProbeStatesBuffer = ResourceDescriptorHeap[g_ProbeStatesIndex];
                if (ProbeStatesBuffer[probeIdx] == PROBE_STATE_INACTIVE)
                {
                    o.positionCS = float4(0.0f, 0.0f, 0.0f, 0.0f);
                    return o;
                }
            }

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
    case DEBUG_NONE:
    case DEBUG_BLOOM:
    case DEBUG_AO:
    case DEBUG_VELOCITY:
    case DEBUG_GI:
    case DEBUG_SHADOW_MASK:
    case DEBUG_ALBEDO:
    case DEBUG_EMISSION:
    case DEBUG_METALLIC:
    case DEBUG_ROUGHNESS:
    case DEBUG_NORMAL:
        o.uv = float2((vertexID << 1) & 2, vertexID & 2);
        o.positionCS = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);

    }

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;
    float2 screenUV = (i.positionCS.xy + 0.5f) * screenSize.zw;
    screenUV = i.uv;

    switch (g_DebugMode)
    {
    case DEBUG_BLOOM:
    {
        float3 bloomColor = SampleTexture2D(g_TargetTexIndex, screenUV, ClampLinearSampler);
        o.target0.rgb = bloomColor;
        break;
    }
    case DEBUG_AO:
    {
        float AO = SampleTexture2D(g_AOIndex, screenUV, ClampLinearSampler);
        o.target0 = AO;
        break;
    }
    case DEBUG_NORMAL:
    {
        float3 normal = SampleNormalWS(screenUV);
        o.target0 = float4(normal, 1.f);
        break;
    }
    case DEBUG_AABB:
    {
        o.target0 = i.color;
        break;
    }
    case DEBUG_VELOCITY:
    {
        float2 velocity = SampleTexture2D(GBuffer5Index, screenUV, ClampPointSampler).rg;
        o.target0 = float4(velocity, 0.f, 1.f);
        break;
    }
    case DEBUG_EMISSION:
    {
        float4 emission = SampleTexture2D(GBuffer4Index, screenUV, ClampPointSampler);
        o.target0 = emission;
        break;
    }
    case DEBUG_ALBEDO:
    {
        float4 albedo = SampleTexture2D(GBuffer0Index, screenUV, ClampPointSampler);
        o.target0 = albedo;
        break;
    }
    case DEBUG_ROUGHNESS:
    {
        float4 albedo = SampleTexture2D(GBuffer1Index, screenUV, ClampPointSampler);
        o.target0 = albedo.b;
        break;
    }
    case DEBUG_METALLIC:
    {
        float4 albedo = SampleTexture2D(GBuffer1Index, screenUV, ClampPointSampler);
        o.target0 = albedo.r;
        break;
    }
    case DEBUG_GIPROBE:
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
                float3 rayDir = DDGIGetProbeRayDir(r, RAYS_PER_PROBE, 0);

                // 2. 计算权重：使用高次幂（如 16 或 32）来获取清晰的细节
                float weight = max(0.0f, dot(N, rayDir));
                weight = pow(weight, 16.0f);

                // 3. 加权累加辐射度
                GIData giData = Elysia_DDGI_LoadRayData(i.instanceID * RAYS_PER_PROBE + r);

                // 排除 Miss 的射线（Distance=10000），防止球体变黑
                if (giData.Distance > 10000.f)
                {
                    finalRadiance += giData.Irradiance * weight;
                    totalWeight += weight;
                }
            }

            // 4. 归一化并输出颜色
            float3 color = (totalWeight > 0.0f)
                               ? (finalRadiance / totalWeight)
                               : float3(0.1f, 0.1f, 0.1f);
            StructuredBuffer<UINT> states = ResourceDescriptorHeap[g_ProbeStatesIndex];
            result.rgb = color;
        }

        o.target0 = result;
        break;
    }
    case DEBUG_SHADOW_MASK:
    {
        float shadowMask = SampleTexture2D(g_TargetTexIndex, screenUV, ClampPointSampler);
        o.target0.rgb = shadowMask;
        break;
    }
    }

    o.target0.a = 1;
    return o;
}