#include "private/SharedCommon.hlsli"

#define GROUP_SIZE 8
#define GOLDEN_RATIO                       1.61803398875f

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SobolNoiseTexIndex;
}

float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(
    int pixel_i,
    int pixel_j,
    int sampleIndex,
    int sampleDimension)
{
    // wrap arguments
    pixel_i = pixel_i & 127;
    pixel_j = pixel_j & 127;
    sampleIndex = sampleIndex & 255;
    sampleDimension = sampleDimension & 255;

    StructuredBuffer<int> SobolBuffer = ResourceDescriptorHeap[g_SobolBufferIndex];
    StructuredBuffer<int> ScramblingTileBuffer = ResourceDescriptorHeap[g_ScramblingTileBufferIndex];
    StructuredBuffer<int> RankingTileBuffer = ResourceDescriptorHeap[g_RankingTileBufferIndex];
    // xor index based on optimized ranking
    int rankedSampleIndex = sampleIndex ^ RankingTileBuffer[sampleDimension + (pixel_i + pixel_j * 128) * 8];

    // fetch value in sequence
    int value = SobolBuffer[sampleDimension + rankedSampleIndex * 256];

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ ScramblingTileBuffer[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8];

    // convert to float and return
    float v = (0.5 + value) / 256.0f;
    return v;
}

float2 SampleRandomVector2D(uint2 pixel)
{
    float2 u = float2(
        fmod(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, 0u) + (
                 frameIndex & 0xFFu) * GOLDEN_RATIO,
             1.0f),
        fmod(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, 1u) + (
                 frameIndex & 0xFFu) * GOLDEN_RATIO,
             1.0f));
    return u;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CalcSobolNoise(uint3 globalID : SV_DispatchThreadID)
{
    RWTexture2D<float2> sobolNoiseTex = ResourceDescriptorHeap[g_SobolNoiseTexIndex];
    sobolNoiseTex[globalID.xy] = SampleRandomVector2D(globalID.xy);
}