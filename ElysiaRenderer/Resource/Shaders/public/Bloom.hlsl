#if EDITOR
#include <private\Common.hlsl>
#else
#include "../private\Common.hlsl"
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    UINT g_bloomRTIndex;
    Vector4 g_ScreenSize;
    
}

[numthreads(8, 8, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 screenUV = ((float2) dispatchThreadID.xy + 0.5f) * g_ScreenSize.zw;
    
    RWTexture2D<float4> outRT = ResourceDescriptorHeap[g_bloomRTIndex];
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    outRT[dispatchThreadID.xy] = SampleTexture2D(OpaqueColorIndex, screenUV, WarpLinearSampler);
}