#include "private\ShadingCommon.hlsl"
#include "private/DDGICommon.hlsli"

struct DispatchArguments
{
    uint ThreadGroupCountX;
    uint ThreadGroupCountY;
    uint ThreadGroupCountZ;
};

struct CompactedRay
{
    float4 Position; // XYZ + Normal.x
    float4 Normal;   // YZ + Data.rg (TexIndices)
    float Distance;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_SourceRayDataBufferIndex;
    UINT g_CompactedRayBufferIndex;
    UINT g_CompactedIndicesBufferIndex;
    UINT g_GlobalCounterBufferIndex;
    UINT g_IndirectArgsBufferIndex;
}