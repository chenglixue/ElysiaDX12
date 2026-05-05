#ifndef OCTAHEDRAL_COMMON_H
#define OCTAHEDRAL_COMMON_H

float2 SignNotZero(float2 v)
{
    return float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// 3D dir normalize to [-1, 1]
float2 OctEncode(float3 n)
{
    n *= rcp((abs(n.x) + abs(n.y) + abs(n.z)));
    float2 result = n.xy;
    if (n.z < 0.0)
    {
        result = (1.0 - abs(result.yx)) * SignNotZero(result);
    }
    return result;
}

// [-1, 1] to normalized 3D dir
float3 OctDecode(float2 f)
{
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += (n.xy >= 0.0) ? -t : t;
    return normalize(n);
}

#endif