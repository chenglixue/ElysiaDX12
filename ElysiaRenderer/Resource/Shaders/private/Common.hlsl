#ifndef COMMON_H
#define COMMON_H

struct FInputParams
{
    float2 PixelPos;
    float4 ScreenPosition;
    float2 ScreenUV;
    float3 ScreenVector;
    half RawDepth;
    half Linear01Depth;
    half LinearEyeDepth;
};

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


#define POSITIVE_INFINITY (asfloat(0x7F800000))
#define NEGATIVE_INFINITY (asfloat(0xFF800000))

#define METER_TO_CENTIMETER		100.0f
#define CENTIMETER_TO_METER		(1.0f / METER_TO_CENTIMETER)
#define KILOMETER_TO_METER  	1000.0f
#define METER_TO_KILOMETER  	(1.0f / KILOMETER_TO_METER)
#define KILOMETER_TO_CENTIMETER	(KILOMETER_TO_METER * METER_TO_CENTIMETER)
#define CENTIMETER_TO_KILOMETER	(1.0f / KILOMETER_TO_CENTIMETER)

#define NearDepthValue (HAS_INVERTED_Z_BUFFER ? 1.0f : 0.0f)
#define FarDepthValue  (HAS_INVERTED_Z_BUFFER ? 0.0f : 1.0f)

const static float MaxHalfFloat = 65504.0f;
const static float Max11BitsFloat = 65024.0f;
const static float Max10BitsFloat = 64512.0f;
const static float3 Max111110BitsFloat3 = float3(Max11BitsFloat, Max11BitsFloat, Max10BitsFloat);

#if POST_PROCESS_ALPHA
#define SceneColorLayout float4
#define CastFloat4ToSceneColorLayout(x) (x)
#define SetSceneColorLayoutToFloat4(dest,value) dest = (value)
#else
#define SceneColorLayout float3
#define CastFloat4ToSceneColorLayout(x) ((x).rgb)
#define SetSceneColorLayoutToFloat4(dest,value) dest.rgb = (value).rgb
#endif

#define SUPPORTS_TEXTURE_EXTERNAL		(COMPILER_GLSL_ES3_1)

#if !SUPPORTS_TEXTURE_EXTERNAL
#define TextureExternal Texture2D
#endif

#ifndef REGISTER
#if COMPILER_HLSLCC
#define REGISTER(x)
#else
#define REGISTER(x)		: register(x)
#endif
#endif

#ifndef SUPPORTS_TEXTURECUBE_ARRAY
#define SUPPORTS_TEXTURECUBE_ARRAY 1
#endif

#if SUPPORTS_TEXTURECUBE_ARRAY == 0
	// Define TextureCubeArray to something which will compile so we can use it in uniform buffers
#define TextureCubeArray TextureCube
#endif

// Control MIP level used for material texture fetches. By default only raytracing 
// shaders (i.e., !PIXELSHADER) use manual MIP level selection. A material shader 
// can opt. in to force a specific MIP level.
//
// * USE_FORCE_TEXTURE_MIP : enable/disable manual MIP level selection
// * FORCED_TEXTURE_MIP    : force a specific MIP level
//
#if COMPUTE_SHADED && !defined(USE_FORCE_TEXTURE_MIP)
#define USE_FORCE_TEXTURE_MIP 0
#endif
#if !PIXELSHADER && !defined(USE_FORCE_TEXTURE_MIP)
#define USE_FORCE_TEXTURE_MIP 1
#endif
#ifndef USE_FORCE_TEXTURE_MIP
#define USE_FORCE_TEXTURE_MIP 0
#endif
#ifndef FORCED_TEXTURE_MIP
#define FORCED_TEXTURE_MIP 0.0f
#endif

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

#ifndef VELOCITY_ENCODE_DEPTH
#define VELOCITY_ENCODE_DEPTH 1
#endif

#if FEATURE_LEVEL >= FEATURE_LEVEL_SM5
#define VELOCITY_ENCODE_GAMMA 1
#else
#define VELOCITY_ENCODE_GAMMA 0
#endif

#if COMPILER_GLSL_ES3_1
#define ENCODED_VELOCITY_TYPE uint4
#else
#define ENCODED_VELOCITY_TYPE float4
#endif

//Tie Editor features to platform support and the COMPILE_SHADERS_FOR_DEVELOPMENT which is set via CVAR.
#define USE_EDITOR_SHADERS (PLATFORM_SUPPORTS_EDITOR_SHADERS && USE_DEVELOPMENT_SHADERS)

// Using SV_ClipDistance has overhead (15% slower base pass in triangle bound test scene on PS4) so projects have to opt-in
#define USE_GLOBAL_CLIP_PLANE (PROJECT_ALLOW_GLOBAL_CLIP_PLANE && !MATERIAL_DOMAIN_POSTPROCESS && !MATERIAL_DOMAIN_UI)

#if RAYHITGROUPSHADER || RAYMISSHADER || RAYCALLABLESHADER

// These built-ins are not available in ray tracing
// Define dummy versions so that ray-tracing materials will at least compile
#define clip(x)
#define ddx(x) 0
#define ddy(x) 0
#define fwidth(x) 0

#endif

#ifndef USE_RAYTRACED_TEXTURE_RAYCONE_LOD
#define USE_RAYTRACED_TEXTURE_RAYCONE_LOD (RAYHITGROUPSHADER)
#endif // USE_RAYTRACED_TEXTURE_RAYCONE_LOD

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

// MaterialFloat Luminance( MaterialFloat3 LinearColor )
// {
// 	return dot( LinearColor, MaterialFloat3( 0.3, 0.59, 0.11 ) );
// }

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

#define POW_CLAMP 0.000001f

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

// Tangent space bias/unbias
// We don't use a function so we can avoid type promotion/ coercion.
#define TangentBias(X)		(X)
#define TangentUnbias(X)	(X)

half Square(half x)
{
    return x * x;
}

half2 Square(half2 x)
{
    return x * x;
}

half3 Square(half3 x)
{
    return x * x;
}

half4 Square(half4 x)
{
    return x * x;
}

half Pow2(half x)
{
    return x * x;
}

half2 Pow2(half2 x)
{
    return x * x;
}

half3 Pow2(half3 x)
{
    return x * x;
}

half4 Pow2(half4 x)
{
    return x * x;
}

half Pow3(half x)
{
    return x * x * x;
}

half2 Pow3(half2 x)
{
    return x * x * x;
}

half3 Pow3(half3 x)
{
    return x * x * x;
}

half4 Pow3(half4 x)
{
    return x * x * x;
}

half E_Pow4(half x)
{
    half xx = x * x;
    return xx * xx;
}

half2 Pow4(half2 x)
{
    half2 xx = x * x;
    return xx * xx;
}

half3 Pow4(half3 x)
{
    half3 xx = x * x;
    return xx * xx;
}

half4 Pow4(half4 x)
{
    half4 xx = x * x;
    return xx * xx;
}

half Pow5(half x)
{
    half xx = x * x;
    return xx * xx * x;
}

half2 Pow5(half2 x)
{
    half2 xx = x * x;
    return xx * xx * x;
}

half3 Pow5(half3 x)
{
    half3 xx = x * x;
    return xx * xx * x;
}

half4 Pow5(half4 x)
{
    half4 xx = x * x;
    return xx * xx * x;
}

half Pow6(half x)
{
    half xx = x * x;
    return xx * xx * xx;
}

half2 Pow6(half2 x)
{
    half2 xx = x * x;
    return xx * xx * xx;
}

half3 Pow6(half3 x)
{
    half3 xx = x * x;
    return xx * xx * xx;
}

half4 Pow6(half4 x)
{
    half4 xx = x * x;
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

inline half2 Panner(half2 uv, half time, half2 speed = half2(1.h, 1.h))
{
    return uv + time * speed;
}
 
inline half3 WhiteOutBlendNormal(half3 normal1, half3 normal2)
{
    half3 result = half3(0.h, 0.h, 0.h);
 
    result = half3(normal1.xy + normal2.xy, normal1.z * normal2.z);
    result = SafeNormalize(result);
 
    return result;
}

inline half Fresnel(half3 normalWS, half3 viewDirWS, half power)
{
    return pow(1.0h - saturate(dot(normalize(normalWS), normalize(viewDirWS))), power);
}

half2 ScaleUVByCenter(half2 uv, half2 scale)
{
    half u = uv.x;
    half v = uv.y;

    half2 temp1 = uv / scale + 0.5h;
    half2 temp2 = 0.5h / scale;

    half2 o = temp1 - temp2;
    return o;
}

half3 Unity_Contrast_float(half3 In, half Contrast)
{
    half midpoint = pow(0.5h, 2.2h);
    half3 Out = (In - midpoint) * Contrast + midpoint;
	
    return Out;
}

half3 Desaturation(half3 inColor, half3 LuminanceFactors, half fraction)
{
    return (1 - fraction) * dot(inColor, LuminanceFactors) + fraction * inColor;
}

half4 Blend_Screen_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = 1.0h - (1.0h - Blend) * (1.0h - Base);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_Burn_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = 1.0h - (1.0h - Blend) / Base;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_Darken_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = min(Blend, Base);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_Difference_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = abs(Blend - Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Dodge_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base / (1.0 - Blend);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Divide_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base / (Blend + 0.000000000001h);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Exclusion_float4(half4 Base, half4 Blend, half Opacity)
{
    float4 Out = Blend + Base - (2.0 * Blend * Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_HardLight_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 result1 = 1.0h - 2.0h * (1.0h - Base) * (1.0h - Blend);
    half4 result2 = 2.0h * Base * Blend;
    half4 zeroOrOne = step(Blend, 0.5h);
    half4 Out = result2 * zeroOrOne + (1.h - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_HardMix_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = step(1.h - Base, Blend);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Lighten_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = max(Blend, Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_LinearBurn_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base + Blend - 1.0h;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_LinearDodge_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base + Blend;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_LinearLight_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Blend < 0.5h ? max(Base + (2.h * Blend) - 1.h, 0) : min(Base + 2.h * (Blend - 0.5h), 1.h);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_LinearLightAddSub_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Blend + 2.0h * Base - 1.0h;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Multiply_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base * Blend;
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Negation_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = 1.0h - abs(1.0h - Blend - Base);
    Out = lerp(Base, Out, Opacity);
    return Out;
}

half4 Blend_Overlay_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 result1 = 1.0h - 2.0h * (1.0h - Base) * (1.0h - Blend);
    half4 result2 = 2.0h * Base * Blend;
    half4 zeroOrOne = step(Base, 0.5h);
    half4 Out = result2 * zeroOrOne + (1.h - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_PinLight_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 check = step(0.5h, Blend);
    half4 result1 = check * max(2.0h * (Base - 0.5h), Blend);
    half4 Out = result1 + (1.0h - check) * min(2.0h * Base, Blend);
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_SoftLight_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 result1 = 2.0h * Base * Blend + Base * Base * (1.0h - 2.0h * Blend);
    half4 result2 = sqrt(Base) * (2.0h * Blend - 1.0h) + 2.0h * Base * (1.0h - Blend);
    half4 zeroOrOne = step(0.5h, Blend);
    half4 Out = result2 * zeroOrOne + (1.h - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;
}

half4 Blend_Subtract_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = Base - Blend;
    Out = lerp(Base, Out, Opacity);

    return Out;

}

half4 Blend_VividLight_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 result1 = 1.0h - (1.0h - Blend) / (2.0h * Base);
    half4 result2 = Blend / (2.0h * (1.0h - Base));
    half4 zeroOrOne = step(0.5h, Base);
    half4 Out = result2 * zeroOrOne + (1.h - zeroOrOne) * result1;
    Out = lerp(Base, Out, Opacity);

    return Out;

}

half4 Blend_Overwrite_float4(half4 Base, half4 Blend, half Opacity)
{
    half4 Out = lerp(Base, Blend, Opacity);
    return Out;
}

half RemapRange(half input, half inputLow, half inputHigh, half outputLow, half outputHigh)
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

float4 TransformViewToHScreen(float3 vpos, half2 screenSize)
{
    float4 cpos = mul(UNITY_MATRIX_P, float4(vpos, 1.h));
    cpos.xy = float2(cpos.x, cpos.y * _ProjectionParams.x) * 0.5h + 0.5h * cpos.w;
    cpos.xy *= screenSize;
    return cpos;
}

void swap(inout half a, inout half b)
{
    half t = a;
    a = b;
    b = t;
}


half DistanceSquared(half2 a, half2 b)
{
    a -= b;
    return dot(a, a);
}

inline half EdgeFade(half2 pos, half value)
{
    half borderDist = min(1.h - max(pos.x, pos.y), min(pos.x, pos.y));
    return saturate(borderDist > value ? 1.h : borderDist / value);
}
inline half ScreenEdgeMask(half2 clipPos)
{
    half yDif = 1.h - abs(clipPos.y);
    half xDif = 1.h - abs(clipPos.x);
	[flatten]
    if (yDif < 0.h || xDif < 0.h)
    {
        return 0.h;
    }
    half t1 = smoothstep(0.h, .2h, yDif);
    half t2 = smoothstep(0.h, .1h, xDif);
    return saturate(t2 * t1);
}

inline half RGB2Lum(half3 rgb)
{
    return (0.299h * rgb.r + 0.587h * rgb.g + 0.114h * rgb.b);
}
inline bool FloatEqApprox(half a, half b)
{
    const float eps = 0.00001h;
    return abs(a - b) < eps;
}

half2 GetMatCapUV(half3 viewDirWS, half3 normalWS)
{
    half3 cameraFoward = -viewDirWS;
    half3 viewUpDir = mul(UNITY_MATRIX_I_V, half4(half3(0.h, 1.h, 0.h), 0.h)).xyz;
    half3 cameraRight = normalize(cross(viewUpDir, cameraFoward));
    half3 cameraUp = normalize(cross(cameraFoward, cameraRight));
 
    half2 uv = mul(float3x3(cameraRight, cameraUp, cameraFoward), normalWS).xy * 0.5h + 0.5h;
    return uv;
}

half3 srgb_to_acescg(half3 col)
{
    half3x3 mat = half3x3(0.61319h, 0.33951h, 0.04737h,
						0.07021h, 0.91634h, 0.01345h,
						0.02062h, 0.10957h, 0.86961h);
    return mul(col, mat);
}

half3 acescg_to_srgb(half3 col)
{
    half3x3 mat = half3x3(1.70505h, -0.62179h, -0.08326h,
						-0.13026h, 1.14080h, -0.01055h,
						-0.02400h, -0.12897h, 1.15297h);
    return mul(col, mat);
}
#endif