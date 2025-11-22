#if EDITOR
#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\SharedCommon.hlsli>
#else
#include "../private\ShadingCommon.hlsl"
#include "../private\Light.hlsl"
#include "../private\LightCommon.hlsl"
#include "../private\SharedCommon.hlsli"
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_bloomRTIndex;
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float4> buffer = ResourceDescriptorHeap[g_bloomRTIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
}