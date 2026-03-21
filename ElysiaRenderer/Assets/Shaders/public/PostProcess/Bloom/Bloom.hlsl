#include "private\Common.hlsl"
#include "private\BloomCommon.hlsli"

#define GROUP_SIZE 8
#define MIPMAP_COUNT 6
cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 g_DestSize;
    Vector4 g_SourceSize;

    UINT g_DestTextureIndex;
    UINT g_SourceTextureIndex;
    UINT g_DownSampleDestTexIndex;

    float g_BloomRadius;
    float g_BloomIntensity;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void BloomKarisDownSample(uint3 id : SV_DispatchThreadID)
{
    uint2 readPos = id.xy;
    uint2 writePos = id.xy;
    if (writePos.x >= g_DestSize.x || writePos.y >= g_DestSize.y)
        return;

    float2 screenUV = (float2(readPos) + 0.5f) * g_DestSize.zw;

    float3 insideTopLeft = SampleTexture2D(g_SourceTextureIndex,
                                           screenUV + float2(-1.f, 1.f) * g_SourceSize.zw,
                                           WarpLinearSampler);
    float3 insideTopRight = SampleTexture2D(g_SourceTextureIndex,
                                            screenUV + float2(1.f, 1.f) * g_SourceSize.zw,
                                            WarpLinearSampler);
    float3 insideBottomLeft = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(-1.f, -1.f) * g_SourceSize.zw,
                                              WarpLinearSampler);
    float3 insideBottomRight = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(1.f, -1.f) * g_SourceSize.zw,
                                               WarpLinearSampler);

    float3 outSideLeftTop = SampleTexture2D(g_SourceTextureIndex,
                                            screenUV + float2(-2.f, 2.f) * g_SourceSize.zw,
                                            WarpLinearSampler);
    float3 outSideLeftMiddle = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(-2.f, 0.f) * g_SourceSize.zw,
                                               WarpLinearSampler);
    float3 outSideLeftBottom = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(-2.f, -2.f) * g_SourceSize.zw,
                                               WarpLinearSampler);
    float3 outSideMiddleTop = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(0.f, 2.f) * g_SourceSize.zw,
                                              WarpLinearSampler);
    float3 outSideMiddle = SampleTexture2D(g_SourceTextureIndex,
                                           screenUV,
                                           WarpLinearSampler);
    float3 outSideMiddleBottom = SampleTexture2D(g_SourceTextureIndex,
                                                 screenUV + float2(0.f, -2.f) * g_SourceSize.zw,
                                                 WarpLinearSampler);
    float3 outSideRightTop = SampleTexture2D(g_SourceTextureIndex,
                                             screenUV + float2(2.f, 2.f) * g_SourceSize.zw,
                                             WarpLinearSampler);
    float3 outSideRightMiddle = SampleTexture2D(g_SourceTextureIndex,
                                                screenUV + float2(2.f, 0.f) * g_SourceSize.zw,
                                                WarpLinearSampler);
    float3 outSideRightBottom = SampleTexture2D(g_SourceTextureIndex,
                                                screenUV + float2(2.f, -2.f) * g_SourceSize.zw,
                                                WarpLinearSampler);

    float3 groupCenterColor = (insideTopLeft + insideTopRight + insideBottomLeft + insideBottomRight) * 0.25f;
    float3 groupTopLeftColor = (outSideLeftTop + outSideLeftMiddle + outSideMiddle + outSideMiddleTop) * 0.25f;
    float3 groupTopRightColor = (outSideRightTop + outSideRightMiddle + outSideMiddle + outSideMiddleTop) * 0.25f;
    float3 groupBottomLeftColor = (outSideLeftMiddle + outSideMiddle + outSideLeftBottom + outSideMiddleBottom) * 0.25f;
    float3 groupBottomRightColor = (outSideMiddle + outSideMiddleBottom + outSideRightMiddle + outSideRightBottom) *
                                   0.25f;

    float weightCenter = GetKarisWeight(groupCenterColor);
    float weightTopLeft = GetKarisWeight(groupTopLeftColor);
    float weightTopRight = GetKarisWeight(groupTopRightColor);
    float weightBottomLeft = GetKarisWeight(groupBottomLeftColor);
    float weightBottomRight = GetKarisWeight(groupBottomRightColor);
    float3 totalColor = groupCenterColor * weightCenter * 0.5f +
                        (groupTopLeftColor * weightTopLeft +
                         groupTopRightColor * weightTopRight +
                         groupBottomLeftColor * weightBottomLeft +
                         groupBottomRightColor * weightBottomRight) * 0.125f;
    float totalWeight = weightCenter * 0.5f + (weightTopLeft + weightTopRight + weightBottomLeft + weightBottomRight) *
                        0.125f;

    float3 finalColor = totalColor / (totalWeight + 1e-6);

    Elysia_Store_Bloom(writePos, g_DestTextureIndex, finalColor);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void BloomWeightedDownSample(uint3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    UINT2 writePos = id.xy;
    if (writePos.x >= g_DestSize.x || writePos.y >= g_DestSize.y)
        return;
    float2 screenUV = ((float2)readPos + 0.5f) * g_DestSize.zw;

    float3 insideTopLeft = SampleTexture2D(g_SourceTextureIndex,
                                           screenUV + float2(-1.f, 1.f) * g_SourceSize.zw,
                                           WarpLinearSampler);
    float3 insideTopRight = SampleTexture2D(g_SourceTextureIndex,
                                            screenUV + float2(1.f, 1.f) * g_SourceSize.zw,
                                            WarpLinearSampler);
    float3 insideBottomLeft = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(-1.f, -1.f) * g_SourceSize.zw,
                                              WarpLinearSampler);
    float3 insideBottomRight = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(1.f, -1.f) * g_SourceSize.zw,
                                               WarpLinearSampler);

    float3 outSideLeftTop = SampleTexture2D(g_SourceTextureIndex,
                                            screenUV + float2(-2.f, 2.f) * g_SourceSize.zw,
                                            WarpLinearSampler);
    float3 outSideLeftMiddle = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(-2.f, 0.f) * g_SourceSize.zw,
                                               WarpLinearSampler);
    float3 outSideLeftBottom = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(-2.f, -2.f) * g_SourceSize.zw,
                                               WarpLinearSampler);
    float3 outSideMiddleTop = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(0.f, 2.f) * g_SourceSize.zw,
                                              WarpLinearSampler);
    float3 outSideMiddle = SampleTexture2D(g_SourceTextureIndex,
                                           screenUV,
                                           WarpLinearSampler);
    float3 outSideMiddleBottom = SampleTexture2D(g_SourceTextureIndex,
                                                 screenUV + float2(0.f, -2.f) * g_SourceSize.zw,
                                                 WarpLinearSampler);
    float3 outSideRightTop = SampleTexture2D(g_SourceTextureIndex,
                                             screenUV + float2(2.f, 2.f) * g_SourceSize.zw,
                                             WarpLinearSampler);
    float3 outSideRightMiddle = SampleTexture2D(g_SourceTextureIndex,
                                                screenUV + float2(2.f, 0.f) * g_SourceSize.zw,
                                                WarpLinearSampler);
    float3 outSideRightBottom = SampleTexture2D(g_SourceTextureIndex,
                                                screenUV + float2(2.f, -2.f) * g_SourceSize.zw,
                                                WarpLinearSampler);

    float3 groupCenterColor = (insideTopLeft + insideTopRight + insideBottomLeft + insideBottomRight) * 0.25f;
    float3 groupTopLeftColor = (outSideLeftTop + outSideLeftMiddle + outSideMiddle + outSideMiddleTop) * 0.25f;
    float3 groupTopRightColor = (outSideRightTop + outSideRightMiddle + outSideMiddle + outSideMiddleTop) * 0.25f;
    float3 groupBottomLeftColor = (outSideLeftMiddle + outSideMiddle + outSideLeftBottom + outSideMiddleBottom) * 0.25f;
    float3 groupBottomRightColor = (outSideMiddle + outSideMiddleBottom + outSideRightMiddle + outSideRightBottom) *
                                   0.25f;

    float3 totalColor = groupCenterColor * 0.5f +
                        (groupTopLeftColor +
                         groupTopRightColor +
                         groupBottomLeftColor +
                         groupBottomRightColor) * 0.125f;

    Elysia_Store_Bloom(writePos, g_DestTextureIndex, totalColor);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void Bloom3x3TentUpSample(uint3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    UINT2 writePos = id.xy;
    if (writePos.x >= g_DestSize.x || writePos.y >= g_DestSize.y)
        return;
    float2 screenUV = ((float2)readPos + 0.5f) * g_DestSize.zw;

    float2 srcTexelSize = g_SourceSize.zw;
    float bloomRadius = max(0.f, g_BloomRadius);
    float2 offset = srcTexelSize * bloomRadius;

    float3 leftTopColor = SampleTexture2D(g_SourceTextureIndex,
                                          screenUV + float2(-1.f, 1.f) * offset,
                                          WarpLinearSampler);
    float3 leftMiddleColor = SampleTexture2D(g_SourceTextureIndex,
                                             screenUV + float2(-1.f, 0.f) * offset,
                                             WarpLinearSampler);
    float3 leftBottomColor = SampleTexture2D(g_SourceTextureIndex,
                                             screenUV + float2(-1.f, -1.f) * offset,
                                             WarpLinearSampler);
    float3 middleTopColor = SampleTexture2D(g_SourceTextureIndex,
                                            screenUV + float2(0.f, 1.f) * offset,
                                            WarpLinearSampler);
    float3 middleColor = SampleTexture2D(g_SourceTextureIndex, screenUV, WarpLinearSampler);
    float3 middleBottomColor = SampleTexture2D(g_SourceTextureIndex,
                                               screenUV + float2(0.f, -1.f) * offset,
                                               WarpLinearSampler);
    float3 rightTopColor = SampleTexture2D(g_SourceTextureIndex,
                                           screenUV + float2(1.f, 1.f) * offset,
                                           WarpLinearSampler);
    float3 rightMiddleColor = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(1.f, 0.f) * offset,
                                              WarpLinearSampler);
    float3 rightBottomColor = SampleTexture2D(g_SourceTextureIndex,
                                              screenUV + float2(1.f, -1.f) * offset,
                                              WarpLinearSampler);

    float3 finalColor = (leftTopColor + leftBottomColor + rightTopColor + rightBottomColor) * rcp(16.f)
                        + (leftMiddleColor + middleTopColor + middleBottomColor + rightMiddleColor) * rcp(8.f)
                        + middleColor * rcp(4.f);
    float3 destColor = SampleTexture2D(g_DownSampleDestTexIndex, screenUV, WarpLinearSampler);
    finalColor += destColor;

    Elysia_Store_Bloom(writePos, g_DestTextureIndex, finalColor);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void BloomBlendSceneColor(uint3 id : SV_DispatchThreadID)
{
    UINT2 readPos = id.xy;
    UINT2 writePos = id.xy;
    float4 screenSize = g_DestSize;
    if (writePos.x >= screenSize.x || writePos.y >= screenSize.y)
        return;

    float2 screenUV = ((float2)readPos + 0.5f) * screenSize.zw;
    float2 onePixel = screenSize.zw;

    float3 top2 = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(0.f, 2.f));
    float3 bottom2 = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(0.f, -2.f));
    float3 left2 = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(-2.f, 0.f));
    float3 right2 = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(2.f, 0.f));
    float3 topLeft = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(-1.f, 1.f));
    float3 topRight = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(1.f, 1.f));
    float3 bottomRight = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(1.f, -1.f));
    float3 bottomLeft = SampleTexture2D(g_SourceTextureIndex, screenUV, onePixel * float2(-1.f, -1.f));

    float3 sceneColor = LoadTexture2D(OpaqueColorIndex, readPos);
    float3 bloomColor = (top2 + bottom2 + left2 + right2 + 2.f * (topLeft + topRight + bottomLeft + bottomRight)) *
                        rcp(12.f);
    bloomColor *= g_BloomIntensity * rcp(MIPMAP_COUNT);
    float3 finalColor = sceneColor + bloomColor;

    Elysia_Store_Bloom(writePos, g_DestTextureIndex, finalColor);
}