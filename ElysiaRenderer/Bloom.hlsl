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
    
}

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{

}