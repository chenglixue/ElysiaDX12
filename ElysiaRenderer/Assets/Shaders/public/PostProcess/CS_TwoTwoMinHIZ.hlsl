#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_TargetTexIndex;
    UINT g_SourceTexIndex;
    float4 g_TargetSize;
    float4 g_SourceSize;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TwoTwoMinHIZ(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 srcBaseCoord = dispatchThreadID.xy * 2;
    const uint2 destCoord = dispatchThreadID.xy;

    if (destCoord.x >= (uint)g_TargetSize.x || destCoord.y >= (uint)g_TargetSize.y)
        return;

    RWTexture2D<float> o = ResourceDescriptorHeap[g_TargetTexIndex];

#if 1
    float4 depths = GatherRedTexture2D(g_SourceTexIndex,
                                       (float2(srcBaseCoord) + 1.0) * g_SourceSize.zw,
                                       ClampPointSampler);

    float minDepth = min(min(depths.x, depths.y), min(depths.z, depths.w));
#else
    const uint2 maxCoord = (uint2)g_SourceSize.xy - 1;

    uint2 coord0 = min(srcBaseCoord + uint2(0, 0), maxCoord);
    uint2 coord1 = min(srcBaseCoord + uint2(0, 1), maxCoord);
    uint2 coord2 = min(srcBaseCoord + uint2(1, 0), maxCoord);
    uint2 coord3 = min(srcBaseCoord + uint2(1, 1), maxCoord);

    // ���в���������������
    float depth0 = srcTex[coord0];
    float depth1 = srcTex[coord1];
    float depth2 = srcTex[coord2];
    float depth3 = srcTex[coord3];

    // ʹ�ò�λ�min����
    float min01 = min(depth0, depth1);
    float min23 = min(depth2, depth3);
    float minDepth = min(min01, min23);
#endif

    o[destCoord] = minDepth;
}