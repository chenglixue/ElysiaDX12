#ifndef COLOR_H
#define COLOR_H

#include "Math.hlsli"

// sRGB
float SRGBToLinear(float c)
{
    float linearRGBLo = c / 12.92;
    float linearRGBHi = PositivePow((c + 0.055) / 1.055, 2.4);
    float linearRGB = (c <= 0.04045) ? linearRGBLo : linearRGBHi;
    return linearRGB;
}

float2 SRGBToLinear(float2 c)
{
    return float2(SRGBToLinear(c.r), SRGBToLinear(c.g));
}

float3 SRGBToLinear(float3 c)
{
    return float3(SRGBToLinear(c.r), SRGBToLinear(c.g), SRGBToLinear(c.b));
}

float4 SRGBToLinear(float4 c)
{
    return float4(SRGBToLinear(c.r), SRGBToLinear(c.g), SRGBToLinear(c.b), c.a);
}

float LinearToSRGB(float c)
{
    return (c <= 0.0031308) ? (c * 12.9232102) : 1.055 * PositivePow(c, 1.0 / 2.4) - 0.055;
}

float2 LinearToSRGB(float2 c)
{
    return float2(LinearToSRGB(c.r), LinearToSRGB(c.g));
}

float3 LinearToSRGB(float3 c)
{
    return float3(LinearToSRGB(c.r), LinearToSRGB(c.g), LinearToSRGB(c.b));
}

float4 LinearToSRGB(float4 c)
{
    return float4(LinearToSRGB(c.r), LinearToSRGB(c.g), LinearToSRGB(c.b), c.a);
}

float3 GetSRGBToLinear(float3 c)
{
    return SRGBToLinear(c);
}

float4 GetSRGBToLinear(float4 c)
{
    return SRGBToLinear(c);
}

float3 GetLinearToSRGB(float3 c)
{
    return LinearToSRGB(c);
}

float4 GetLinearToSRGB(float4 c)
{
    return LinearToSRGB(c);
}

// Neutral tonemapping (Hable/Hejl/Frostbite)
// Input is linear RGB
// More accuracy to avoid NaN on extremely high values.
float3 NeutralCurve(float3 x, float a, float b, float c, float d, float e, float f)
{
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

//Extremely high values cause NaN output when using fp16, we clamp to avoid the performace hit of switching to fp32
//The overflow happens in (x * (a * x + b) + d * f) of the NeutralCurve, highest value that avoids fp16 precision errors is ~571.56873
//Since whiteScale is constant (~1.31338) max input is ~435.18712
float3 NeutralTonemap(float3 x)
{
	// Make sure negative channels are clamped to 0.0 as this neutral tonemapper can't deal with them properly (unlike ACES)
    x = max((0.0).xxx, x);

    // Tonemap
    const float a = 0.2;
    const float b = 0.29;
    const float c = 0.24;
    const float d = 0.272;
    const float e = 0.02;
    const float f = 0.3;
    const float whiteLevel = 5.3;
    const float whiteClip = 1.0;

    float3 whiteScale = (1.0).xxx / NeutralCurve(whiteLevel, a, b, c, d, e, f);
    x = NeutralCurve(x * whiteScale, a, b, c, d, e, f);
    x *= whiteScale;

    // Post-curve white point adjustment
    x /= whiteClip.xxx;

    return x;
}

float Max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float3 TonemapWithWeight(float3 c, float w)
{
    return c * (w * rcp(Max3(c.r, c.g, c.b) + 1.0));
}

float3 TonemapInvert(float3 c)
{
    return c * rcp(1.0 - Max3(c.r, c.g, c.b));
}
 
float3 AMDTonemapInvert(float3 c)
{
    return TonemapInvert(TonemapWithWeight(c, 0.25) +
      TonemapWithWeight(c, 0.25) +
      TonemapWithWeight(c, 0.25) +
      TonemapWithWeight(c, 0.25));
}


#endif