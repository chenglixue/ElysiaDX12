#ifndef COLOR_H
#define COLOR_H

#include "Math.hlsli"
#include "Common.hlsl"

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

float3 ToneMapFilmicALU(in float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
    return color;
}

float3 uncharted2_tonemap_partial(float3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
float3 uncharted2_filmic(float3 v)
{
    float exposure_bias = 2.0f;
    float3 curr = uncharted2_tonemap_partial(v * exposure_bias);
    float3 W = 11.2f;
    float3 white_scale = 1.0f / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

float3 ApplyGamma(float3 color)
{
    color.xyz = pow(color.xyz, 1.0f / 2.2f);
    return color;
}

float3 ApplyPQ(float3 color)
{
    // Apply ST2084 curve
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 cp = pow(abs(color.xyz), m1);
    color.xyz = pow((c1 + c2 * cp) / (1 + c3 * cp), m2);
    return color;
}

float3 ApplyscRGBScale(float3 color, float minLuminancePerNits, float maxLuminancePerNits)
{
    color.xyz = (color.xyz * (maxLuminancePerNits - minLuminancePerNits)) + float3(minLuminancePerNits, minLuminancePerNits, minLuminancePerNits);
    return color;
}


#endif