#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_BlurTexIndex;
    UINT g_ReinterleaveAOTexIndex;
    UINT g_AOImportanceTexIndex;
    float g_Sharpness_Inv;

    UINT g_BlurRadius;
    bool g_IsBlur;

    UINT g_ActiveLayerIndex;
    Vector4 g_SourceTexIndices;
    Vector4 g_TargetTexIndices;
}

void Elysia_BlurAO_StoreOutput(UINT index, UINT2 id, min16float2 val)
{
    RWTexture2D<min16float2> o = ResourceDescriptorHeap[index];
    o[id.xy] = val;
}

min16float4 UnpackEdges(min16float _packedVal)
{
    uint packedVal = (uint)(_packedVal * 255.5);
    min16float4 edgesLRTB;
    edgesLRTB.x = min16float((packedVal >> 6) & 0x03) / 3.0;
    // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = min16float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = min16float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = min16float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_Sharpness_Inv);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void AOEdgeSensitiveBlur(uint3 id : SV_DispatchThreadID)
{
    UINT layerIndex = g_ActiveLayerIndex;
    UINT AOIndex = g_SourceTexIndices[layerIndex];
    // RWTexture2D<float2> o = ResourceDescriptorHeap[g_TargetTexIndices[layerIndex]];

    // UINT AOIndex = g_ReinterleaveAOTexIndex;
    UINT BlurIndex = g_TargetTexIndices[layerIndex];

    min16float2 centerData = LoadTexture2D(AOIndex, id);
    min16float centerAO = centerData.r;
    min16float4 centerEdge = UnpackEdges(centerData.g);

    min16float L = LoadTexture2D(AOIndex, id + int2(-1, 0) * g_BlurRadius).r;
    min16float R = LoadTexture2D(AOIndex, id + int2(1, 0) * g_BlurRadius).r;
    min16float T = LoadTexture2D(AOIndex, id + int2(0, -1) * g_BlurRadius).r;
    min16float B = LoadTexture2D(AOIndex, id + int2(0, 1) * g_BlurRadius).r;

    min16float weightSum = 1.0;
    min16float totalAO = centerAO;
    totalAO += L * centerEdge.x;
    totalAO += R * centerEdge.y;
    totalAO += T * centerEdge.z;
    totalAO += B * centerEdge.w;

    weightSum += centerEdge.x;
    weightSum += centerEdge.y;
    weightSum += centerEdge.z;
    weightSum += centerEdge.w;

    min16float blurredAO = totalAO / weightSum;

    Elysia_BlurAO_StoreOutput(BlurIndex,
                              id,
                              float2(g_IsBlur ? blurredAO : centerData.x, centerData.y));
}