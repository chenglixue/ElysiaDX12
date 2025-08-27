#ifndef MATH_H
#define MATH_H

#include "Macros.hlsli"

// These types are used for material translator generated code, or any functions the translated code can call
#if PIXELSHADER && !FORCE_MATERIAL_FLOAT_FULL_PRECISION
#define MaterialFloat half
#define MaterialFloat2 half2
#define MaterialFloat3 half3
#define MaterialFloat4 half4
#define MaterialFloat3x3 half3x3
#define MaterialFloat4x4 half4x4 
#define MaterialFloat4x3 half4x3 
#else
	// Material translated vertex shader code always uses floats, 
	// Because it's used for things like world position and UVs
#define MaterialFloat float
#define MaterialFloat2 float2
#define MaterialFloat3 float3
#define MaterialFloat4 float4
#define MaterialFloat3x3 float3x3
#define MaterialFloat4x4 float4x4 
#define MaterialFloat4x3 float4x3 
#endif

#ifndef COMPUTE_SHADED
#define COMPUTE_SHADED 0
#endif

struct FloatDeriv
{
    float Value;
    float Ddx;
    float Ddy;
};

struct FloatDeriv2
{
    float2 Value;
    float2 Ddx;
    float2 Ddy;
};

struct FloatDeriv3
{
    float3 Value;
    float3 Ddx;
    float3 Ddy;
};

struct FloatDeriv4
{
    float4 Value;
    float4 Ddx;
    float4 Ddy;
};

FloatDeriv ConstructFloatDeriv(float InValue, float InDdx, float InDdy)
{
    FloatDeriv Ret;
    Ret.Value = InValue;
    Ret.Ddx = InDdx;
    Ret.Ddy = InDdy;
    return Ret;
}

FloatDeriv2 ConstructFloatDeriv2(float2 InValue, float2 InDdx, float2 InDdy)
{
    FloatDeriv2 Ret;
    Ret.Value = InValue;
    Ret.Ddx = InDdx;
    Ret.Ddy = InDdy;
    return Ret;
}

FloatDeriv3 ConstructFloatDeriv3(float3 InValue, float3 InDdx, float3 InDdy)
{
    FloatDeriv3 Ret;
    Ret.Value = InValue;
    Ret.Ddx = InDdx;
    Ret.Ddy = InDdy;
    return Ret;
}

FloatDeriv4 ConstructFloatDeriv4(float4 InValue, float4 InDdx, float4 InDdy)
{
    FloatDeriv4 Ret;
    Ret.Value = InValue;
    Ret.Ddx = InDdx;
    Ret.Ddy = InDdy;
    return Ret;
}

// In HLSL, fmod is implemented as 'Lhs - trunc(Lhs / Rhs) * Rhs'
// In some cases, using floor rather than trunc is better
float FmodFloor(float Lhs, float Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float2 FmodFloor(float2 Lhs, float2 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float3 FmodFloor(float3 Lhs, float3 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float4 FmodFloor(float4 Lhs, float4 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float VectorSum(float V)
{
    return V;
}
float VectorSum(float2 V)
{
    return V.x + V.y;
}
float VectorSum(float3 V)
{
    return V.x + V.y + V.z;
}
float VectorSum(float4 V)
{
    return V.x + V.y + V.z + V.w;
}

float ClampToHalfFloatRange(float X)
{
    return clamp(X, float(0), MaxHalfFloat);
}
float2 ClampToHalfFloatRange(float2 X)
{
    return clamp(X, float(0).xx, MaxHalfFloat.xx);
}
float3 ClampToHalfFloatRange(float3 X)
{
    return clamp(X, float(0).xxx, MaxHalfFloat.xxx);
}
float4 ClampToHalfFloatRange(float4 X)
{
    return clamp(X, float(0).xxxx, MaxHalfFloat.xxxx);
}

float Luminance(float3 LinearColor)
{
    return dot(LinearColor, float3(0.3, 0.59, 0.11));
}

float length2(float2 v)
{
    return dot(v, v);
}
float length2(float3 v)
{
    return dot(v, v);
}
float length2(float4 v)
{
    return dot(v, v);
}

uint Mod(uint a, uint b)
{
#if FEATURE_LEVEL >= FEATURE_LEVEL_ES3_1
    return a % b;
#else
	return a - (b * (uint)((float)a / (float)b));
#endif
}

uint2 Mod(uint2 a, uint2 b)
{
#if FEATURE_LEVEL >= FEATURE_LEVEL_ES3_1
    return a % b;
#else
	return a - (b * (uint2)((float2)a / (float2)b));
#endif
}

uint3 Mod(uint3 a, uint3 b)
{
#if FEATURE_LEVEL >= FEATURE_LEVEL_ES3_1
    return a % b;
#else
	return a - (b * (uint3)((float3)a / (float3)b));
#endif
}

// Clamp the base, so it's never <= 0.0f (INF/NaN).
MaterialFloat ClampedPow(MaterialFloat X, MaterialFloat Y)
{
    return pow(max(abs(X), POW_CLAMP), Y);
}
MaterialFloat2 ClampedPow(MaterialFloat2 X, MaterialFloat2 Y)
{
    return pow(max(abs(X), MaterialFloat2(POW_CLAMP, POW_CLAMP)), Y);
}
MaterialFloat3 ClampedPow(MaterialFloat3 X, MaterialFloat3 Y)
{
    return pow(max(abs(X), MaterialFloat3(POW_CLAMP, POW_CLAMP, POW_CLAMP)), Y);
}
MaterialFloat4 ClampedPow(MaterialFloat4 X, MaterialFloat4 Y)
{
    return pow(max(abs(X), MaterialFloat4(POW_CLAMP, POW_CLAMP, POW_CLAMP, POW_CLAMP)), Y);
}

// Pow function that will return 0 if Base is <= 0 (or small enough to yield a floating point special).
// This is done to prevent floating point specials when compilers expands pow into exp(Exponent * log(Base)).
MaterialFloat PositiveClampedPow(MaterialFloat Base, MaterialFloat Exponent)
{
    return (Base <= 2.980233e-8f) ? 0.0f : pow(Base, Exponent);
}
MaterialFloat2 PositiveClampedPow(MaterialFloat2 Base, MaterialFloat2 Exponent)
{
    return MaterialFloat2(PositiveClampedPow(Base.x, Exponent.x), PositiveClampedPow(Base.y, Exponent.y));
}
MaterialFloat3 PositiveClampedPow(MaterialFloat3 Base, MaterialFloat3 Exponent)
{
    return MaterialFloat3(PositiveClampedPow(Base.xy, Exponent.xy), PositiveClampedPow(Base.z, Exponent.z));
}
MaterialFloat4 PositiveClampedPow(MaterialFloat4 Base, MaterialFloat4 Exponent)
{
    return MaterialFloat4(PositiveClampedPow(Base.xy, Exponent.xy), PositiveClampedPow(Base.zw, Exponent.zw));
}

float DDX(float Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddx(Input);
#endif
}

float2 DDX(float2 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddx(Input);
#endif
}

float3 DDX(float3 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddx(Input);
#endif
}

float4 DDX(float4 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddx(Input);
#endif
}

float DDY(float Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddy(Input);
#endif
}

float2 DDY(float2 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddy(Input);
#endif
}

float3 DDY(float3 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddy(Input);
#endif
}

float4 DDY(float4 Input)
{
#if USE_FORCE_TEXTURE_MIP
    return 0;
#else
    return ddy(Input);
#endif
}

float Square(float x)
{
    return x * x;
}

float2 Square(float2 x)
{
    return x * x;
}

float3 Square(float3 x)
{
    return x * x;
}

float4 Square(float4 x)
{
    return x * x;
}

float Pow2(float x)
{
    return x * x;
}

float2 Pow2(float2 x)
{
    return x * x;
}

float3 Pow2(float3 x)
{
    return x * x;
}

float4 Pow2(float4 x)
{
    return x * x;
}

float Pow3(float x)
{
    return x * x * x;
}

float2 Pow3(float2 x)
{
    return x * x * x;
}

float3 Pow3(float3 x)
{
    return x * x * x;
}

float4 Pow3(float4 x)
{
    return x * x * x;
}

float E_Pow4(float x)
{
    float xx = x * x;
    return xx * xx;
}

float2 Pow4(float2 x)
{
    float2 xx = x * x;
    return xx * xx;
}

float3 Pow4(float3 x)
{
    float3 xx = x * x;
    return xx * xx;
}

float4 Pow4(float4 x)
{
    float4 xx = x * x;
    return xx * xx;
}

float Pow5(float x)
{
    float xx = x * x;
    return xx * xx * x;
}

float2 Pow5(float2 x)
{
    float2 xx = x * x;
    return xx * xx * x;
}

float3 Pow5(float3 x)
{
    float3 xx = x * x;
    return xx * xx * x;
}

float4 Pow5(float4 x)
{
    float4 xx = x * x;
    return xx * xx * x;
}

float Pow6(float x)
{
    float xx = x * x;
    return xx * xx * xx;
}

float2 Pow6(float2 x)
{
    float2 xx = x * x;
    return xx * xx * xx;
}

float3 Pow6(float3 x)
{
    float3 xx = x * x;
    return xx * xx * xx;
}

float4 Pow6(float4 x)
{
    float4 xx = x * x;
    return xx * xx * xx;
}

// Only valid for x >= 0
MaterialFloat AtanFast(MaterialFloat x)
{
	// Minimax 3 approximation
	MaterialFloat3 A = x < 1 ? MaterialFloat3(x, 0, 1) : MaterialFloat3(1 / x, 0.5 * PI, -1);
    return A.y + A.z * (((-0.130234 * A.x - 0.0954105) * A.x + 1.00712) * A.x - 0.00001203333);
}

/** Converts a linear input value into a value to be stored in the light attenuation buffer. */
MaterialFloat EncodeLightAttenuation(MaterialFloat InColor)
{
	// Apply a 1/2 power to the input, which allocates more bits for the darks and prevents banding
	// Similar to storing colors in gamma space, except this uses less instructions than a pow(x, 1/2.2)
    return sqrt(InColor);
}

/** Converts a linear input value into a value to be stored in the light attenuation buffer. */
MaterialFloat4 EncodeLightAttenuation(MaterialFloat4 InColor)
{
    return sqrt(InColor);
}

/** Converts value stored in the light attenuation buffer into a linear light attenuation value. */
MaterialFloat DecodeLightAttenuation(MaterialFloat InColor)
{
    return Square(InColor);
}

/** Converts value stored in the light attenuation buffer into a linear light attenuation value. */
MaterialFloat4 DecodeLightAttenuation(MaterialFloat4 InColor)
{
    return Square(InColor);
}

// Like RGBM but this can be interpolated.
MaterialFloat4 RGBTEncode(MaterialFloat3 Color)
{
	MaterialFloat4 RGBT;
	MaterialFloat Max = max(max(Color.r, Color.g), max(Color.b, 1e-6));
	MaterialFloat RcpMax = rcp(Max);
    RGBT.rgb = Color.rgb * RcpMax;
    RGBT.a = Max * rcp(1.0 + Max);
    return RGBT;
}

MaterialFloat3 RGBTDecode(MaterialFloat4 RGBT)
{
    RGBT.a = RGBT.a * rcp(1.0 - RGBT.a);
    return RGBT.rgb * RGBT.a;
}



MaterialFloat4 RGBMEncode(MaterialFloat3 Color)
{
    Color *= 1.0 / 64.0;
	
    float4 rgbm;
    rgbm.a = saturate(max(max(Color.r, Color.g), max(Color.b, 1e-6)));
    rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
    rgbm.rgb = Color / rgbm.a;
    return rgbm;
}

MaterialFloat4 RGBMEncodeFast(MaterialFloat3 Color)
{
	// 0/0 result written to fixed point buffer goes to zero
	MaterialFloat4 rgbm;
    rgbm.a = dot(Color, 255.0 / 64.0);
    rgbm.a = ceil(rgbm.a);
    rgbm.rgb = Color / rgbm.a;
    rgbm *= MaterialFloat4(255.0 / 64.0, 255.0 / 64.0, 255.0 / 64.0, 1.0 / 255.0);
    return rgbm;
}

MaterialFloat3 RGBMDecode(MaterialFloat4 rgbm, MaterialFloat MaxValue)
{
    return rgbm.rgb * (rgbm.a * MaxValue);
}

MaterialFloat3 RGBMDecode(MaterialFloat4 rgbm)
{
    return rgbm.rgb * (rgbm.a * 64.0f);
}

MaterialFloat4 RGBTEncode8BPC(MaterialFloat3 Color, MaterialFloat Range)
{
	MaterialFloat Max = max(max(Color.r, Color.g), max(Color.b, 1e-6));
    Max = min(Max, Range);

	MaterialFloat4 RGBT;
    RGBT.a = (Range + 1) / Range * Max / (1 + Max);

	// quantise alpha to 8 bit.
    RGBT.a = ceil(RGBT.a * 255.0) / 255.0;
    Max = RGBT.a / (1 + 1 / Range - RGBT.a);

	MaterialFloat RcpMax = rcp(Max);
    RGBT.rgb = Color.rgb * RcpMax;
    return RGBT;
}

MaterialFloat3 RGBTDecode8BPC(MaterialFloat4 RGBT, MaterialFloat Range)
{
    RGBT.a = RGBT.a / (1 + 1 / Range - RGBT.a);
    return RGBT.rgb * RGBT.a;
}

/** 
 * Use this function to compute the pow() in the specular computation.
 * This allows to change the implementation depending on platform or it easily can be replaced by some approxmation.
 */
MaterialFloat PhongShadingPow(MaterialFloat X, MaterialFloat Y)
{
	// The following clamping is done to prevent NaN being the result of the specular power computation.
	// Clamping has a minor performance cost.

	// In HLSL pow(a, b) is implemented as exp2(log2(a) * b).

	// For a=0 this becomes exp2(-inf * 0) = exp2(NaN) = NaN.

	// As seen in #TTP 160394 "QA Regression: PS3: Some maps have black pixelated artifacting."
	// this can cause severe image artifacts (problem was caused by specular power of 0, lightshafts propagated this to other pixels).
	// The problem appeared on PlayStation 3 but can also happen on similar PC NVidia hardware.

	// In order to avoid platform differences and rarely occuring image atrifacts we clamp the base.

	// Note: Clamping the exponent seemed to fix the issue mentioned TTP but we decided to fix the root and accept the
	// minor performance cost.

    return ClampedPow(X, Y);
}

inline float2 Panner(float2 uv, float time, float2 speed = float2(1., 1.))
{
    return uv + time * speed;
}
 
inline float3 WhiteOutBlendNormal(float3 normal1, float3 normal2)
{
    float3 result = float3(0., 0., 0.);
 
    result = float3(normal1.xy + normal2.xy, normal1.z * normal2.z);
    result = normalize(result);
 
    return result;
}

inline float Fresnel(float3 normalWS, float3 viewDirWS, float power)
{
    return pow(1.0h - saturate(dot(normalize(normalWS), normalize(viewDirWS))), power);
}

float2 ScaleUVByCenter(float2 uv, float2 scale)
{
    float u = uv.x;
    float v = uv.y;

    float2 temp1 = uv / scale + 0.5;
    float2 temp2 = 0.5 / scale;

    float2 o = temp1 - temp2;
    return o;
}

float3 Unity_Contrast_float(float3 In, float Contrast)
{
    float midpoint = pow(0.5, 2.2);
    float3 Out = (In - midpoint) * Contrast + midpoint;
	
    return Out;
}

float3 Desaturation(float3 inColor, float3 LuminanceFactors, float fraction)
{
    return (1 - fraction) * dot(inColor, LuminanceFactors) + fraction * inColor;
}

float4 Blend_Screen_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = 1.0 - (1.0 - Blend) * (1.0 - Base);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_Burn_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = 1.0 - (1.0 - Blend) / Base;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_Darken_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = min(Blend, Base);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_Difference_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = abs(Blend - Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Dodge_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base / (1.0 - Blend);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Divide_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base / (Blend + 0.000000000001);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Exclusion_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Blend + Base - (2.0 * Blend * Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_HardLight_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 result1 = 1.0 - 2.0 * (1.0 - Base) * (1.0 - Blend);
    float4 result2 = 2.0 * Base * Blend;
    float4 zeroOrOne = step(Blend, 0.5);
    float4 Out = result2 * zeroOrOne + (1. - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_HardMix_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = step(1. - Base, Blend);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Lighten_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = max(Blend, Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_LinearBurn_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base + Blend - 1.0;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_LinearDodge_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base + Blend;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_LinearLight_float4(float4 Base, float4 Blend, float Opacity)
{

    float4 Out = select(Blend < 0.5, max(Base + (2. * Blend) - 1., 0), min(Base + 2. * (Blend - 0.5), 1.));
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_LinearLightAddSub_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Blend + 2.0 * Base - 1.0;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Multiply_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base * Blend;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Negation_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = 1.0 - abs(1.0 - Blend - Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

float4 Blend_Overlay_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 result1 = 1.0 - 2.0 * (1.0 - Base) * (1.0 - Blend);
    float4 result2 = 2.0 * Base * Blend;
    float4 zeroOrOne = step(Base, 0.5);
    float4 Out = result2 * zeroOrOne + (1. - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_PinLight_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 check = step(0.5, Blend);
    float4 result1 = check * max(2.0 * (Base - 0.5), Blend);
    float4 Out = result1 + (1.0 - check) * min(2.0 * Base, Blend);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_SoftLight_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 result1 = 2.0 * Base * Blend + Base * Base * (1.0 - 2.0 * Blend);
    float4 result2 = sqrt(Base) * (2.0 * Blend - 1.0) + 2.0 * Base * (1.0 - Blend);
    float4 zeroOrOne = step(0.5, Blend);
    float4 Out = result2 * zeroOrOne + (1. - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

float4 Blend_Subtract_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = Base - Blend;
    Out = lerp(Base, Out, Opacity);

    return Out;

}

float4 Blend_VividLight_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 result1 = 1.0 - (1.0 - Blend) / (2.0 * Base);
    float4 result2 = Blend / (2.0 * (1.0 - Base));
    float4 zeroOrOne = step(0.5, Base);
    float4 Out = result2 * zeroOrOne + (1. - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;

}

float4 Blend_Overwrite_float4(float4 Base, float4 Blend, float Opacity)
{
    float4 Out = lerp(Base, Blend, Opacity);
    return Out;
}

float RemapRange(float input, float inputLow, float inputHigh, float outputLow, float outputHigh)
{
    return (input - inputLow) / (inputHigh - inputLow) * (outputHigh - outputLow) + outputLow;
}

bool IsInfinityFar(float rawDepth)
{
#if UNITY_REVERSED_Z
	// Case for platforms with REVERSED_Z, such as D3D.
	if (rawDepth < 0.00001f)
		return true;
#else
	// Case for platforms without REVERSED_Z, such as OpenGL.
    if (rawDepth > 0.9999f)
        return true;
#endif
    return false;
}

float4 TransformViewToHScreen(float3 vpos, float2 screenSize, float4x4 M_P)
{
    float4 cpos = mul(M_P, float4(vpos, 1.));
    cpos.xy = float2(cpos.x, cpos.y) * 0.5 + 0.5 * cpos.w;
    cpos.xy *= screenSize;
    return cpos;
}

void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}


float DistanceSquared(float2 a, float2 b)
{
    a -= b;
    return dot(a, a);
}

inline float EdgeFade(float2 pos, float value)
{
    float borderDist = min(1. - max(pos.x, pos.y), min(pos.x, pos.y));
    return saturate(borderDist > value ? 1. : borderDist / value);
}
inline float ScreenEdgeMask(float2 clipPos)
{
    float yDif = 1. - abs(clipPos.y);
    float xDif = 1. - abs(clipPos.x);
	[flatten]
    if (yDif < 0. || xDif < 0.)
    {
        return 0.;
    }
    float t1 = smoothstep(0., .2, yDif);
    float t2 = smoothstep(0., .1, xDif);
    return saturate(t2 * t1);
}

inline float RGB2Lum(float3 rgb)
{
    return (0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b);
}
inline bool FloatEqApprox(float a, float b)
{
    const float eps = 0.00001;
    return abs(a - b) < eps;
}

float2 GetMatCapUV(float3 viewDirWS, float3 normalWS, float4x4 M_V)
{
    float3 cameraFoward = -viewDirWS;
    float3 viewUpDir = mul(M_V, float4(float3(0., 1., 0.), 0.)).xyz;
    float3 cameraRight = normalize(cross(viewUpDir, cameraFoward));
    float3 cameraUp = normalize(cross(cameraFoward, cameraRight));
 
    float2 uv = mul(float3x3(cameraRight, cameraUp, cameraFoward), normalWS).xy * 0.5 + 0.5;
    return uv;
}

float3 srgb_to_acescg(float3 col)
{
    float3x3 mat = float3x3(0.61319, 0.33951, 0.04737,
						0.07021, 0.91634, 0.01345,
						0.02062, 0.10957, 0.86961);
    return mul(col, mat);
}

float3 acescg_to_srgb(float3 col)
{
    float3x3 mat = float3x3(1.70505, -0.62179, -0.08326,
						-0.13026, 1.14080, -0.01055,
						-0.02400, -0.12897, 1.15297);
    return mul(col, mat);
}

float3 GetScreenVectorWS(float3 cameraPosWS, float3 targetPosWS)
{
    return normalize(targetPosWS - cameraPosWS);
}

#endif