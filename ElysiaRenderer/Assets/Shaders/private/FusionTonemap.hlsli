#ifndef FUSION_TONEMAP_H
#define FUSION_TONEMAP_H

#include "Color.hlsl"

// TonemapMode in TonemapUtility.h. LMP stays on the old Reinhard curve:
// LPM has no scalar inverse wired up yet.
static const uint kFusionTonemapNeutral = 0;
static const uint kFusionTonemapLMP = 1;
static const uint kFusionTonemapAMD = 2;
static const uint kFusionTonemapACES = 3;
static const uint kFusionTonemapUncharted2 = 4;
static const uint kFusionTonemapDX11 = 5;
static const uint kFusionTonemapFilmic = 6;

// Grey luminance through the display operator. Colored crosstalk (AMD) drops
// out because R = G = B. Filmic is already grey-preserving in AP1.
float FusionOperator(float lum,
                     uint mode,
                     float filmSlope,
                     float filmToe,
                     float filmShoulder,
                     float filmBlackClip,
                     float filmWhiteClip)
{
    lum = max(lum, 0.0f);
    float3 rgb = float3(lum, lum, lum);

    BRANCH
    switch (mode)
    {
    case kFusionTonemapNeutral:
        return NeutralTonemap(rgb).r;
    case kFusionTonemapAMD:
        return AMDTonemapper(rgb).r;
    case kFusionTonemapACES:
        return ACESFilmTone(rgb).r;
    case kFusionTonemapUncharted2:
        return Uncharted2Tonemap(rgb).r;
    case kFusionTonemapDX11:
        return DX11DSKTone(rgb).r;
    case kFusionTonemapFilmic:
        return FilmToneMap(rgb, filmSlope, filmToe, filmShoulder, filmBlackClip, filmWhiteClip).r;
    default:
        return lum / (1.0f + lum);
    }
}

// UE ExposureFusionTonemap: sqrt spreads the curve so weights around 0.5 stay useful.
float FusionEncodeLuminance(float lum,
                            uint mode,
                            float filmSlope,
                            float filmToe,
                            float filmShoulder,
                            float filmBlackClip,
                            float filmWhiteClip)
{
    return sqrt(max(FusionOperator(lum, mode, filmSlope, filmToe, filmShoulder, filmBlackClip, filmWhiteClip), 0.0f));
}

// Inverse of FusionEncodeLuminance. LMP is closed form. The others bisect the
// same operator the display uses, in log2 space.
float FusionDecodeLuminance(float encoded,
                            uint mode,
                            float filmSlope,
                            float filmToe,
                            float filmShoulder,
                            float filmBlackClip,
                            float filmWhiteClip)
{
    BRANCH
    if (mode == kFusionTonemapLMP)
    {
        encoded = min(max(encoded, 0.0f), 0.999f);
        float squared = encoded * encoded;
        return squared / max(1.0f - squared, 1e-5f);
    }

    encoded = max(encoded, 0.0f);
    const float kHi = exp2(12.0f);
    float yHi = FusionOperator(kHi, mode, filmSlope, filmToe, filmShoulder, filmBlackClip, filmWhiteClip);
    float y = min(encoded * encoded, max(yHi, 0.0f));
    if (y <= FusionOperator(0.0f, mode, filmSlope, filmToe, filmShoulder, filmBlackClip, filmWhiteClip))
        return 0.0f;

    float lo = -16.0f;
    float hi = 12.0f;
    [loop]
    for (int i = 0; i < 16; ++i)
    {
        float mid = 0.5f * (lo + hi);
        float sample = FusionOperator(exp2(mid), mode, filmSlope, filmToe, filmShoulder, filmBlackClip, filmWhiteClip);
        if (sample < y)
            lo = mid;
        else
            hi = mid;
    }
    return exp2(0.5f * (lo + hi));
}

#endif
