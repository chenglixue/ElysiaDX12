#ifndef SSSR_COMMON_H
#define SSSR_COMMON_H

#include <private/SharedCommon.hlsli>

bool SSSR_IsThreadOutWorkArea(UINT2 ThreadPos, float4 AreaSize)
{
    BRANCH
    if (ThreadPos.x >= AreaSize.x || ThreadPos.y >= AreaSize.y)
    {
        return true;
    }

    return false;
}

uint SSSR_BitfieldExtract(uint src, uint off, uint bits)
{
    uint mask = (1 << bits) - 1;
    return (src >> off) & mask;
}

uint SSSR_BitfieldInsert(uint src, uint ins, uint bits)
{
    uint mask = (1 << bits) - 1;
    return (ins & mask) | (src & (~mask));
}

//  LANE TO 8x8 MAPPING
//  ===================
//  00 01 08 09 10 11 18 19
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f
//  20 21 28 29 30 31 38 39
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f
uint2 SSSR_RemapLane8x8(uint lane)
{
    return uint2(SSSR_BitfieldInsert(SSSR_BitfieldExtract(lane, 2u, 3u), lane, 1u),
                 SSSR_BitfieldInsert(SSSR_BitfieldExtract(lane, 3u, 3u),
                                     SSSR_BitfieldExtract(lane, 1u, 2u),
                                     2u));
}

UINT PackRayData(UINT2 coord, bool copyHorizontal, bool copyVertical, bool copyDiagonal)
{
    uint ray_x_15bit = coord.x & 0b111111111111111;
    uint ray_y_14bit = coord.y & 0b11111111111111;
    uint copy_horizontal_1bit = copyHorizontal ? 1 : 0;
    uint copy_vertical_1bit = copyVertical ? 1 : 0;
    uint copy_diagonal_1bit = copyDiagonal ? 1 : 0;

    uint packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (
                      ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

void UnpackRayData(UINT packed,
                   out UINT2 rayCoord,
                   out bool copyHorizontal,
                   out bool copyVertical,
                   out bool copyDiagonal)
{
    rayCoord.x = (packed >> 0) & 0b111111111111111;
    rayCoord.y = (packed >> 15) & 0b11111111111111;
    copyHorizontal = (packed >> 29) & 0b1;
    copyVertical = (packed >> 30) & 0b1;
    copyDiagonal = (packed >> 31) & 0b1;
}

float2 SSSR_GetMipResolution(float2 screen_dimensions, int mip_level)
{
    return screen_dimensions * pow(0.5, mip_level);
}

void SSSR_InitialAdvanceRay(float3 origin, float3 direction, float3 inv_direction, float2 current_mip_resolution, float2 current_mip_resolution_inv, float2 floor_offset, float2 uv_offset, out float3 CurrPos, out float CurrT)
{
    float2 current_mip_position = current_mip_resolution * origin.xy;
    float2 XYPlane = floor(current_mip_position) + floor_offset;

    XYPlane = XYPlane * current_mip_resolution_inv + uv_offset;

    float2 t = XYPlane * inv_direction.xy - origin.xy * inv_direction.xy;
    CurrT = min(t.x, t.y);
    CurrPos = origin + CurrT * direction;
}

bool SSSR_AdvanceRay(float3 origin, float3 direction, float3 inv_direction, float2 current_mip_position, float2 current_mip_resolution_inv, float2 floor_offset, float2 uv_offset, float surface_z, inout float3 position, inout float current_t) {
    // Create boundary planes
    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;
    float3 boundary_planes = float3(xy_plane, surface_z);

    // Intersect ray with the half box that is pointing away from the ray origin.
    // o + d * t = p' => t = (p' - o) / d
    float3 t = boundary_planes * inv_direction - origin * inv_direction;

    // Prevent using z plane when shooting out of the depth buffer.
#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
    t.z = direction.z < 0 ? t.z : FLT_MAX;
#else
    t.z = direction.z > 0 ? t.z : FLT_MAX;
#endif

    // Choose nearest intersection with a boundary.
    float t_min = min(min(t.x, t.y), t.z);

#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
    // Larger z means closer to the camera.
    bool above_surface = surface_z < position.z;
#else
    // Smaller z means closer to the camera.
    bool above_surface = surface_z > position.z;
#endif

    // Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
    // We use the asuint comparison to avoid NaN / Inf logic, also we actually care about bitwise equality here to see if t_min is the t.z we fed into the min3 above.
    bool skipped_tile = asuint(t_min) != asuint(t.z) && above_surface;

    // Make sure to only advance the ray if we're still above the surface.
    current_t = above_surface ? t_min : current_t;

    // Advance ray
    position = origin + current_t * direction;

    return skipped_tile;
}


#endif
