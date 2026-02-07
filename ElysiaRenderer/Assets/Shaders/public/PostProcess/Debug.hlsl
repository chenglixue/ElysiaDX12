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
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};


uint3 GetProbeGridCoord(uint probeIndex)
{
    uint3 gridCoord;
    gridCoord.x = probeIndex % 16;
    gridCoord.y = (probeIndex / 16) % 4;
    gridCoord.z = probeIndex / (16 * 4);
    return gridCoord;
}
float3 GetProbeWorldPosition(uint probeIndex)
{
    uint3 coord = GetProbeGridCoord(probeIndex);
    // 位置 = 起点 + 索引 * 步长
    return g_GridOrigin + (float3(coord) * g_GridSpacing);
}

PSInput VS(VSInput i, UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput)0;

    switch (g_DebugMode)
    {
    case DEBUG_GI:
    {
        o.instanceID = i.instanceID;
        o.probeCenterWS = GetProbeWorldPosition(i.instanceID);

        o.positionWS = i.positionOS * g_ProbeRadius + o.probeCenterWS;

        o.positionCS = mul(float4(o.positionWS, 1.f), viewProjMatrix);
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
    case DEBUG_GI:
    {
        float3 N = normalize(i.positionWS - i.probeCenterWS);
        float2 octUV = OctEncode(N);
        float2 uv = GetProbeUV(i.instanceID, octUV, g_GridDimensions.xyz, 6.0);

        float3 irradiance = SampleTexture2D(g_IrradianceTexIndex, uv, ClampPointSampler);
        o.target0 = float4(irradiance, 1.f);
        break;
    }
    }
    return o;
}