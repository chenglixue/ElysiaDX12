#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_FullScreenSize;

    UINT g_BlurTexIndex;
    UINT g_ReinterleaveAOTexIndex;
    UINT g_AOImportanceTexIndex;
    float g_Sharpness_Inv;

    UINT g_BlurRadius;
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
    UINT AOIndex = g_ReinterleaveAOTexIndex;
    UINT BlurIndex = g_BlurTexIndex;

    float2 centerData = LoadTexture2D(AOIndex, id);
    float centerAO = centerData.r;
    float4 centerEdge = UnpackEdges(centerData.g);

    float L = LoadTexture2D(AOIndex, id + int2(-1, 0) * g_BlurRadius).r;
    float R = LoadTexture2D(AOIndex, id + int2(1, 0) * g_BlurRadius).r;
    float T = LoadTexture2D(AOIndex, id + int2(0, -1) * g_BlurRadius).r;
    float B = LoadTexture2D(AOIndex, id + int2(0, 1) * g_BlurRadius).r;

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

    [branch]
    if (g_BlurRadius == 1)
    {
        float2 uv = (id.xy + 0.5) * g_FullScreenSize.zw;
        float importance = SampleTexture2D(g_AOImportanceTexIndex, uv, ClampLinearSampler).r;
        float mixFactor = smoothstep(0.2, 0.8, importance);
        float finalAO = lerp(blurredAO, centerAO, mixFactor);
        Elysia_BlurAO_StoreOutput(BlurIndex,
                                  id,
                                  float2(g_IsBlur ? finalAO : centerData.x, centerData.y));
    }
    else
    {
        Elysia_BlurAO_StoreOutput(BlurIndex,
                                  id,
                                  float2(g_IsBlur ? blurredAO : centerData.x, centerData.y));
    }

}