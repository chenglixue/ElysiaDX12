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

PSInput VS(VSInput i)
{
    PSInput o = (PSInput)0;

    o.instanceID = i.instanceID;
    o.probeCenterWS = GetProbeWorldPosition(i.instanceID);

    o.positionWS = i.positionOS * g_ProbeRadius + o.probeCenterWS;

    o.positionCS = mul(float4(o.positionWS, 1.f), viewProjMatrix);

    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    float3 N = normalize(i.positionWS - i.probeCenterWS);
    float2 octUV = OctEncode(N);
    float2 uv = GetProbeUV(i.instanceID, octUV, g_GridDimensions.xyz, 6.0);

    float3 irradiance = SampleTexture2D(g_IrradianceTexIndex, uv, ClampPointSampler);
    o.target0 = float4(irradiance, 1.f);
    return o;
}