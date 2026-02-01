#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_DeinterleaveBlurTexIndices;
    Vector4 g_DeinterleaveAOTexIndices;
    float g_Sharpness_Inv;
    bool g_IsBlur;
}

void Elysia_BlurAO_StoreOutput(UINT index, UINT2 id, float2 val)
{
    RWTexture2D<float2> o = ResourceDescriptorHeap[index];
    o[id.xy] = val;
}

float4 UnpackEdges(float _packedVal)
{
    uint packedVal = (uint)(_packedVal * 255.5);
    float4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;
    // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_Sharpness_Inv);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AOEdgeSensitiveBlur(uint3 id : SV_DispatchThreadID)
{
    UINT layerIndex = id.z;
    UINT AOLayerHeapIndex = g_DeinterleaveAOTexIndices[layerIndex];
    UINT BlurLayerHeapIndex = g_DeinterleaveBlurTexIndices[layerIndex];

    float2 centerData = LoadTexture2D(AOLayerHeapIndex, id);
    float centerAO = centerData.r;
    float4 centerEdge = UnpackEdges(centerData.g);

    float L = LoadTexture2D(AOLayerHeapIndex, id + int2(-1, 0)).r;
    float R = LoadTexture2D(AOLayerHeapIndex, id + int2(1, 0)).r;
    float T = LoadTexture2D(AOLayerHeapIndex, id + int2(0, -1)).r;
    float B = LoadTexture2D(AOLayerHeapIndex, id + int2(0, 1)).r;

    float weightSum = 1.0;
    float totalAO = centerAO;
    totalAO += L * centerEdge.x;
    totalAO += R * centerEdge.y;
    totalAO += T * centerEdge.z;
    totalAO += B * centerEdge.w;

    weightSum += centerEdge.x;
    weightSum += centerEdge.y;
    weightSum += centerEdge.z;
    weightSum += centerEdge.w;

    float blurredAO = totalAO / weightSum;

    Elysia_BlurAO_StoreOutput(BlurLayerHeapIndex,
                              id,
                              float2(g_IsBlur ? blurredAO : centerData.x, centerData.y));
}