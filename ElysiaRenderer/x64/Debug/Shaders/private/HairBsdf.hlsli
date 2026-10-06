#ifndef HAIR_BSDF_H
#define HAIR_BSDF_H

#include "SharedCommon.hlsli"

float3 KajiyaKayDiffuse(FDecodeGBufferData GBuffer, float3 L, float3 V, half3 N, float Shadow)
{
    float3 o = (float3)0;

    float3 sqrtAlbedo = sqrt(GBuffer.BaseColor);

    float MinValue = 0.0001f;
    float3 scatterTint = pow(GBuffer.BaseColor * rcp(Luminance(max(GBuffer.BaseColor, MinValue))), 1.f - Shadow);

    float3 scatter = (dot(N, L) + 1) / FOUR_PI;

    o += sqrtAlbedo * scatter * scatterTint;
    return o;
}

float ModifiedBesselI0(float x)
{
    if (x == 0.f)
    {
        return 1.f;
    }

    float sum = 0.f;
    float term = 1.f;
    int k = 0;

    [unroll(64)]
    for (; term > FLT_EPS;)
    {
        sum += term;
        k ++;
        term *= (x * x) / (4.0 * k * k);
    }

    return sum;
}

float Hair_F(float CosTheta)
{
    const float n = 1.55f;
    const float F0 = Pow2(1 - n) * rcp(Pow2(1 + n));
    const float F_Fun = F0 + (1 - F0) * Pow5(1 - CosTheta);

    return F_Fun;
}
float Hair_g(float roughness, float Theta)
{
    return exp(-0.5f * Pow2(Theta) / Pow2(roughness)) * rcp(sqrt(TWO_PI) * roughness);
}

float3 HairShading(FDecodeGBufferData GBuffer, float3 L, float3 V, half3 T, float Shadow)
{
    float3 o = (float3)0;

    float backLit = GBuffer.CustomData.z;
    float clampedRoughness = clamp(GBuffer.Roughness, 0.05f, 1.f);
    if (g_EnableMultiScatter)
    {
        o += KajiyaKayDiffuse(GBuffer, L, V, T, Shadow);
    }

    const float VoL = dot(V, L);
    const float sinThetaL = clamp(dot(T, L), -1.f, 1.f);
    const float sinThetaV = clamp(dot(T, V), -1.f, 1.f);
    const float cosThetaL = sqrt(1 - Pow2(sinThetaL));
    const float cosThetaV = sqrt(1 - Pow2(sinThetaV));
    const float cosThetaD = cos(abs(asin(sinThetaV) - asin(sinThetaL)) * 0.5f);

    const float3 Lp = L - T * sinThetaL;
    const float3 Vp = V - T * sinThetaV;
    const float cosPhi = dot(Lp, Vp) * rsqrt(dot(Lp, Lp) * dot(Vp, Vp) + 1e-4);
    const float cosHalfPhi = sqrt(saturate(0.5f + 0.5f * cosPhi));

    float n_prime = 1.19 / cosThetaD + 0.36 * cosThetaD;
    float Shift = 0.035;
    float Alpha[] =
    {
        -Shift * 2,
        Shift,
        Shift * 4,
    };

    if (g_EnableR)
    {
        const float v = Pow2(clampedRoughness);

        float M = Hair_g(v * cosHalfPhi * sqrt(2), sinThetaL + sinThetaV - Alpha[0]);

        const float N = 0.25f * cosHalfPhi;

        const float3 H = normalize(V + L);
        const float A = Hair_F(dot(V, H));

        const float specularScale = GBuffer.Specular * 2.f;

        o += M * N * A * specularScale * lerp(1.f, backLit, saturate(-VoL));
    }

    if (g_EnableTT)
    {
        float beta = 0.5 * Pow2(clampedRoughness);

        float M = Hair_g(beta, sinThetaL + sinThetaV - Alpha[1]);

        float a = rcp(n_prime);
        float h = cosHalfPhi * (1 + a * (0.6 - 0.8 * cosPhi));

        float T = pow(GBuffer.BaseColor, sqrt(1 - Pow2(h * a)) * rcp(2.f * cosThetaD));
        float f = Hair_F(cosThetaD * sqrt(1 - Pow2(h)));
        float A = T * Pow2(1 - f);

        float Dp = exp(-3.65f * cosPhi - 3.98);

        o += M * Dp * A * backLit;
    }

    if (g_EnableTRT)
    {
        float beta = 2.f * Pow2(clampedRoughness);
        float M = Hair_g(beta, sinThetaL + sinThetaV - Alpha[2]);

        float T = pow(GBuffer.BaseColor, 0.8f * rcp(cosThetaD));
        float Dp = exp(17.f * cosPhi - 16.78f);
        float N = T * Dp;

        float f = Hair_F(cosThetaD * 0.5f);
        float A = Pow2(1 - f) * f;

        o += M * N * A;
    }

    o = -min(-o, 0.f);
    return o;
}
#endif