#if EDITOR
#include <private\Color.hlsl>
#else
#include "../private\Color.hlsl"
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    uint blitterTextureIndex;
    
    bool u_shoulder;
    bool u_con;
    bool u_soft;
    bool u_con2;
    bool u_clip;
    bool u_scaleOnly;
    uint u_displayMode;
    matrix u_inputToOutputMatrix;
    uint4 u_ctl[24];
}

#define A_GPU 1
#define A_HLSL 1
#define A_HALF 1
//#include "private/ffx_a.h"


#define LPM_NO_SETUP 1
uint4 LpmFilterCtl(uint i)
{
    return u_ctl[i];
}

//#include "private/ffx_lpm.h"

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(UINT vertexID : SV_VertexID)
{
    PSInput o = (PSInput) 0;
    
    if (vertexID == 0)
    {
        o.positionCS = float4(-1.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 0.0f);
    }
    else if (vertexID == 1)
    {
        o.positionCS = float4(3.0f, 1.0f, 1.0f, 1.0f);
        o.uv = float2(2.0f, 0.0f);
    }
    else
    {
        o.positionCS = float4(-1.0f, -3.0f, 1.0f, 1.0f);
        o.uv = float2(0.0f, 2.0f);
    }
    
    return o;
}

#define AF1 float
#define AF2 float2
#define AF3 float3
#define AF4 float4
#define AP1 bool
#define AU4 uint4
#define AF4_AU4 uint4
#define AF1_AU1(x) asfloat(AU1(x))
#define AF2_AU2(x) asfloat(AU2(x))
#define AF3_AU3(x) asfloat(AU3(x))
#define AF4_AU4(x) asfloat(AU4(x))
AF1 AMax3F1(AF1 x, AF1 y, AF1 z)
{
    return max(x, max(y, z));
}
AF1 ARcpF1(AF1 x)
{
    return rcp(x);
}
AF1 AF1_x(AF1 a)
{
    return AF1(a);
}
AF2 AF2_x(AF1 a)
{
    return AF2(a, a);
}
AF3 AF3_x(AF1 a)
{
    return AF3(a, a, a);
}
AF4 AF4_x(AF1 a)
{
    return AF4(a, a, a, a);
}
#define AF1_(a) AF1_x(AF1(a))
#define AF2_(a) AF2_x(AF1(a))
#define AF3_(a) AF3_x(AF1(a))
#define AF4_(a) AF4_x(AF1(a))
AF1 ASatF1(AF1 x)
{
    return saturate(x);
}
AF2 ASatF2(AF2 x)
{
    return saturate(x);
}
AF3 ASatF3(AF3 x)
{
    return saturate(x);
}
AF4 ASatF4(AF4 x)
{
    return saturate(x);
}

void LpmMap(inout AF1 colorR, inout AF1 colorG, inout AF1 colorB, // Input and output color.
    AF3 lumaW, // Luma coef for RGB working space.
    AF3 lumaT, // Luma coef for crosstalk mapping (can be working or output color-space depending on usage case).
    AF3 rcpLumaT, // 1/lumaT.
    AF3 saturation, // Saturation powers.
    AF1 contrast, // Contrast power.
    AP1 shoulder, // Using shoulder tuning (should be a compile-time immediate).
    AF1 shoulderContrast, // Shoulder power.
    AF2 toneScaleBias, // Other tonemapping parameters.
    AF3 crosstalk, // Crosstalk scaling for over-exposure color shaping.
    AP1 con, // Use first RGB conversion matrix (should be a compile-time immediate), if 'soft' then 'con' must be true also.
    AF3 conR, AF3 conG, AF3 conB, // RGB conversion matrix (working to output space conversion).
    AP1 soft, // Use soft gamut mapping (should be a compile-time immediate).
    AF2 softGap, // {x,(1-x)/(x*0.693147180559)}, where 'x' is gamut mapping soft fall-off amount. 
    AP1 con2, // Use last RGB conversion matrix (should be a compile-time immediate).
    AP1 clip, // Use clipping on last conversion matrix.
    AP1 scaleOnly, // Do scaling only (special case for 709 HDR to scRGB).
    AF3 con2R, AF3 con2G, AF3 con2B)
{ // Secondary RGB conversion matrix.
    //------------------------------------------------------------------------------------------------------------------------------
      // Grab original RGB ratio (RCP, 3x MUL, MAX3).
    AF1 rcpMax = ARcpF1(AMax3F1(colorR, colorG, colorB));AF1 ratioR = colorR * rcpMax;AF1 ratioG = colorG * rcpMax;AF1 ratioB = colorB * rcpMax;
    // Apply saturation, ratio must be max 1.0 for this to work right (3x EXP2, 3x LOG2, 3x MUL).
    ratioR = pow(ratioR, AF1_(saturation.r));
    ratioG = pow(ratioG, AF1_(saturation.g));
    ratioB = pow(ratioB, AF1_(saturation.b));
    //------------------------------------------------------------------------------------------------------------------------------
      // Tonemap luma, note this uses the original color, so saturation is luma preserving.
      // If not using 'con' this uses the output space luma directly to avoid needing extra constants.
      // Note 'soft' should be a compile-time immediate (so no branch) (3x MAD).
    AF1 luma;
    if (soft)
        luma = colorG * AF1_(lumaW.g) + (colorR * AF1_(lumaW.r) + (colorB * AF1_(lumaW.b)));
    else
        luma = colorG * AF1_(lumaT.g) + (colorR * AF1_(lumaT.r) + (colorB * AF1_(lumaT.b)));
    luma = pow(luma, AF1_(contrast)); // (EXP2, LOG2, MUL).
    AF1 lumaShoulder = shoulder ? pow(luma, AF1_(shoulderContrast)) : luma; // Optional (EXP2, LOG2, MUL).
    luma = luma * ARcpF1(lumaShoulder * AF1_(toneScaleBias.x) + AF1_(toneScaleBias.y)); // (MAD, MUL, RCP).
    //------------------------------------------------------------------------------------------------------------------------------
      // If running soft clipping (this should be a compile-time immediate so branch will not exist).
    if (soft)
    {
        // The 'con' should be a compile-time immediate so branch will not exist.
        // Use of 'con' is implied if soft-falloff is enabled, but using the check here to make finding bugs easy.
        if (con)
        {
            // Converting ratio instead of color. Change of primaries (9x MAD).
            colorR = ratioR;
            colorG = ratioG;
            colorB = ratioB;
            ratioR = colorR * AF1_(conR.r) + (colorG * AF1_(conR.g) + (colorB * AF1_(conR.b)));
            ratioG = colorG * AF1_(conG.g) + (colorR * AF1_(conG.r) + (colorB * AF1_(conG.b)));
            ratioB = colorB * AF1_(conB.b) + (colorG * AF1_(conB.g) + (colorR * AF1_(conB.r)));
            // Convert ratio to max 1 again (RCP, 3x MUL, MAX3).
            rcpMax = ARcpF1(AMax3F1(ratioR, ratioG, ratioB));
            ratioR *= rcpMax;
            ratioG *= rcpMax;
            ratioB *= rcpMax;
        }
        //------------------------------------------------------------------------------------------------------------------------------
           // Absolute gamut mapping converted to soft falloff (maintains max 1 property).
           //  g = gap {0 to g} used for {-inf to 0} input range
           //          {g to 1} used for {0 to 1} input range
           //  x >= 0 := y = x * (1-g) + g
           //  x < 0  := g * 2^(x*h)
           //  Where h=(1-g)/(g*log(2)) --- where log() is the natural log
           // The {g,h} above is passed in as softGap.
           // Soft falloff (3x MIN, 3x MAX, 9x MAD, 3x EXP2).
        ratioR = min(max(AF1_(softGap.x), ASatF1(ratioR * AF1_(-softGap.x) + ratioR)),
            ASatF1(AF1_(softGap.x) * exp2(ratioR * AF1_(softGap.y))));
        ratioG = min(max(AF1_(softGap.x), ASatF1(ratioG * AF1_(-softGap.x) + ratioG)),
            ASatF1(AF1_(softGap.x) * exp2(ratioG * AF1_(softGap.y))));
        ratioB = min(max(AF1_(softGap.x), ASatF1(ratioB * AF1_(-softGap.x) + ratioB)),
            ASatF1(AF1_(softGap.x) * exp2(ratioB * AF1_(softGap.y))));
    }
    //------------------------------------------------------------------------------------------------------------------------------
      // Compute ratio scaler required to hit target luma (4x MAD, 1 RCP).
    AF1 lumaRatio = ratioR * AF1_(lumaT.r) + ratioG * AF1_(lumaT.g) + ratioB * AF1_(lumaT.b);
    // This is limited to not clip.
    AF1 ratioScale = ASatF1(luma * ARcpF1(lumaRatio));
    // Assume in gamut, compute output color (3x MAD).
    colorR = ASatF1(ratioR * ratioScale);
    colorG = ASatF1(ratioG * ratioScale);
    colorB = ASatF1(ratioB * ratioScale);
    // Capability per channel to increase value (3x MAD).
    // This factors in crosstalk factor to avoid multiplies later.
    //  '(1.0-ratio)*crosstalk' optimized to '-crosstalk*ratio+crosstalk' 
    AF1 capR = AF1_(-crosstalk.r) * colorR + AF1_(crosstalk.r);
    AF1 capG = AF1_(-crosstalk.g) * colorG + AF1_(crosstalk.g);
    AF1 capB = AF1_(-crosstalk.b) * colorB + AF1_(crosstalk.b);
    // Compute amount of luma needed to add to non-clipped channels to make up for clipping (3x MAD).
    AF1 lumaAdd = ASatF1((-colorB) * AF1_(lumaT.b) + ((-colorR) * AF1_(lumaT.r) + ((-colorG) * AF1_(lumaT.g) + luma)));
    // Amount to increase keeping over-exposure ratios constant and possibly exceeding clipping point (4x MAD, 1 RCP).
    AF1 t = lumaAdd * ARcpF1(capG * AF1_(lumaT.g) + (capR * AF1_(lumaT.r) + (capB * AF1_(lumaT.b))));
    // Add amounts to base color but clip (3x MAD).
    colorR = ASatF1(t * capR + colorR);
    colorG = ASatF1(t * capG + colorG);
    colorB = ASatF1(t * capB + colorB);
    // Compute amount of luma needed to add to non-clipped channel to make up for clipping (3x MAD).
    lumaAdd = ASatF1((-colorB) * AF1_(lumaT.b) + ((-colorR) * AF1_(lumaT.r) + ((-colorG) * AF1_(lumaT.g) + luma)));
    // Add to last channel (3x MAD).
    colorR = ASatF1(lumaAdd * AF1_(rcpLumaT.r) + colorR);
    colorG = ASatF1(lumaAdd * AF1_(rcpLumaT.g) + colorG);
    colorB = ASatF1(lumaAdd * AF1_(rcpLumaT.b) + colorB);
    //------------------------------------------------------------------------------------------------------------------------------
      // The 'con2' should be a compile-time immediate so branch will not exist.
      // Last optional place to convert from smaller to larger gamut (or do clipped conversion).
      // For the non-soft-falloff case, doing this after all other mapping saves intermediate re-scaling ratio to max 1.0.
    if (con2)
    {
        // Change of primaries (9x MAD).
        ratioR = colorR;
        ratioG = colorG;
        ratioB = colorB;
        if (clip)
        {
            colorR = ASatF1(ratioR * AF1_(con2R.r) + (ratioG * AF1_(con2R.g) + (ratioB * AF1_(con2R.b))));
            colorG = ASatF1(ratioG * AF1_(con2G.g) + (ratioR * AF1_(con2G.r) + (ratioB * AF1_(con2G.b))));
            colorB = ASatF1(ratioB * AF1_(con2B.b) + (ratioG * AF1_(con2B.g) + (ratioR * AF1_(con2B.r))));
        }
        else
        {
            colorR = ratioR * AF1_(con2R.r) + (ratioG * AF1_(con2R.g) + (ratioB * AF1_(con2R.b)));
            colorG = ratioG * AF1_(con2G.g) + (ratioR * AF1_(con2G.r) + (ratioB * AF1_(con2G.b)));
            colorB = ratioB * AF1_(con2B.b) + (ratioG * AF1_(con2B.g) + (ratioR * AF1_(con2B.r)));
        }
    }
    //------------------------------------------------------------------------------------------------------------------------------
    if (scaleOnly)
    {
        colorR *= AF1_(con2R.r);
        colorG *= AF1_(con2R.r);
        colorB *= AF1_(con2R.r);
    }
}
void LpmFilter(
    // Input and output color.
    inout AF1 colorR, inout AF1 colorG, inout AF1 colorB,
    // Path control should all be compile-time immediates.
    AP1 shoulder, // Using shoulder tuning.
    // Prefab "LPM_CONFIG_" start, use the same as used for LpmSetup().
    AP1 con, // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
    AP1 soft, // Use soft gamut mapping.
    AP1 con2, // Use last RGB conversion matrix.
    AP1 clip, // Use clipping in last conversion matrix.
    AP1 scaleOnly)
{ // Scale only for last conversion matrix (used for 709 HDR to scRGB).
    // Grab control block, what is unused gets dead-code removal.
    AU4 map0 = LpmFilterCtl(0);
    AU4 map1 = LpmFilterCtl(1);
    AU4 map2 = LpmFilterCtl(2);
    AU4 map3 = LpmFilterCtl(3);
    AU4 map4 = LpmFilterCtl(4);
    AU4 map5 = LpmFilterCtl(5);
    AU4 map6 = LpmFilterCtl(6);
    AU4 map7 = LpmFilterCtl(7);
    AU4 map8 = LpmFilterCtl(8);
    AU4 map9 = LpmFilterCtl(9);
    AU4 mapA = LpmFilterCtl(10);
    AU4 mapB = LpmFilterCtl(11);
    AU4 mapC = LpmFilterCtl(12);
    AU4 mapD = LpmFilterCtl(13);
    AU4 mapE = LpmFilterCtl(14);
    AU4 mapF = LpmFilterCtl(15);
    AU4 mapG = LpmFilterCtl(16);
    AU4 mapH = LpmFilterCtl(17);
    AU4 mapI = LpmFilterCtl(18);
    AU4 mapJ = LpmFilterCtl(19);
    AU4 mapK = LpmFilterCtl(20);
    AU4 mapL = LpmFilterCtl(21);
    AU4 mapM = LpmFilterCtl(22);
    AU4 mapN = LpmFilterCtl(23);
    LpmMap(colorR, colorG, colorB,
        AF3(AF4_AU4(map6).g, AF4_AU4(map6).b, AF4_AU4(map6).a), // lumaW
        AF3(AF4_AU4(map1).b, AF4_AU4(map1).a, AF4_AU4(map2).r), // lumaT
        AF3(AF4_AU4(map3).r, AF4_AU4(map3).g, AF4_AU4(map3).b), // rcpLumaT
        AF3(AF4_AU4(map0).r, AF4_AU4(map0).g, AF4_AU4(map0).b), // saturation
        AF4_AU4(map0).a, // contrast
        shoulder,
        AF4_AU4(map6).r, // shoulderContrast
        AF2(AF4_AU4(map1).r, AF4_AU4(map1).g), // toneScaleBias
        AF3(AF4_AU4(map2).g, AF4_AU4(map2).b, AF4_AU4(map2).a), // crosstalk
        con,
        AF3(AF4_AU4(map7).b, AF4_AU4(map7).a, AF4_AU4(map8).r), // conR
        AF3(AF4_AU4(map8).g, AF4_AU4(map8).b, AF4_AU4(map8).a), // conG
        AF3(AF4_AU4(map9).r, AF4_AU4(map9).g, AF4_AU4(map9).b), // conB
        soft,
        AF2(AF4_AU4(map7).r, AF4_AU4(map7).g), // softGap
        con2, clip, scaleOnly,
        AF3(AF4_AU4(map3).a, AF4_AU4(map4).r, AF4_AU4(map4).g), // con2R
        AF3(AF4_AU4(map4).b, AF4_AU4(map4).a, AF4_AU4(map5).r), // con2G
        AF3(AF4_AU4(map5).g, AF4_AU4(map5).b, AF4_AU4(map5).a));
} // con2B

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    float4 color = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);
    color = mul(u_inputToOutputMatrix, color);
    
    color.r = max(0, color.r);
    color.g = max(0, color.g);
    color.b = max(0, color.b);
    
    //float3 tonemapColor = NeutralTonemap(color);
    //tonemapColor = ToneMapFilmicALU(color);
    //tonemapColor = uncharted2_filmic(color);

    #ifdef A_GPU
    LpmFilter(color.r, color.g, color.b, u_shoulder, false, false, false, false, false);
    #endif
    
    o.target0 = float4(color.rgb, 1.f);
    
    return o;
}

float4 BlitPS(PSInput i) : SV_TARGET
{   
    half4 blitterValue = SampleTexture2D(blitterTextureIndex, i.uv, ClampLinearSampler);
    
    return blitterValue;
}
