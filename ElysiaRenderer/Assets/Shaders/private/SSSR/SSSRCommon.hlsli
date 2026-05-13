#ifndef SSSR_COMMON_H
#define SSSR_COMMON_H

#include <private/SharedCommon.hlsli>

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
    UINT copyHorizontal1Bit = copyHorizontal ? 1 : 0;
    UINT copyVertical1Bit = copyVertical ? 1 : 0;
    UINT copyDiagonal1Bit = copyDiagonal ? 1 : 0;
    UINT coord14Bit = coord.y & 0b11111111111111;
    UINT coord15Bit = coord.x & 0b111111111111111;

    return (copyHorizontal1Bit << 31) |
           (copyVertical1Bit << 30) |
           (copyDiagonal1Bit << 29) |
           (coord14Bit << 15) |
           (coord15Bit << 0);

}
#endif