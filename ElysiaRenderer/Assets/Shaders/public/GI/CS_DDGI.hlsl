#include "private\ShadingCommon.hlsl"

#define GROUP_SIZE 8

#define DEBUG_NONE 0
#define DEBUG_AO 1

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void DDGI(uint3 dispatchThreadID : SV_DispatchThreadID)
{

}