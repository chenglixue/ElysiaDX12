#include "private\ShadingCommon.hlsl"
#include "private/SSSR/SSSRCommon.hlsli"

#define GROUP_SIZE 8

cbuffer PassConstant : register(b0, perPassSpace)
{
    float4 g_DestSize;
}

groupshared uint g_TileCount;

float GetRoughness(UINT2 readPos);

void DoTileClassify(UINT2 readPos, UINT2 groupThreadID, float roughness);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TileClassify(uint2 groupID : SV_GroupID,
                  uint groupIndex : SV_GroupIndex)
{
    // 0-64 ->
    UINT2 groupThreadID = SSSR_RemapLane8x8(groupIndex);
    uint2 globalID = groupID * 8 + groupThreadID;

    UINT readPos = globalID;

    float roughness = GetRoughness(globalID);

    DoTileClassify(readPos, groupThreadID, roughness);
}

float GetRoughness(UINT2 readPos)
{
    return LoadTexture2D(GBuffer1Index, readPos).b;
}

void DoTileClassify(UINT2 readPos, UINT2 groupThreadID, float roughness)
{
    g_TileCount = 0;

    bool isFirstActiveThreadInWave = WaveIsFirstLane();

    bool needRay = !(readPos.x >= g_DestSize.x || readPos.y >= g_DestSize.y);
}