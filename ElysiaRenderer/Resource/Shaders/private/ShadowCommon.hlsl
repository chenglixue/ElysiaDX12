#ifndef SHADOW_COMMON_H
#define SHADOW_COMMON_H

//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
float3 GetShadowPosOffset(in float NoL, in float3 normal, in float shadowMapSize)
{
    const float offsetScale = 4.0f;
    float texelSize = 2.0f / shadowMapSize;
    float nmlOffsetScale = saturate(1.0f - NoL);
    return texelSize * offsetScale * nmlOffsetScale * normal;
}

#endif