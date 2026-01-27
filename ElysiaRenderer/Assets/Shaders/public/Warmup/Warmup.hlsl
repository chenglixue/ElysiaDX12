#include "private\ShadingCommon.hlsl"

cbuffer PassConstant : register(b0, perMaterialSpace)
{
    UINT g_TargetTexIndex;
}