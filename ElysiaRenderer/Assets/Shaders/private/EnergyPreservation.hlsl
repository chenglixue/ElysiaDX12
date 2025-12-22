#ifndef ENERGY_PRESERVATION_H
#define ENERGY_PRESERVATION_H

#pragma once

#include "SharedCommon.hlsli"

struct FBxDFEnergyTerms
{
    float3 W; // overall weight to scale the lobe BxDF by to ensure energy conservation
    float3 E; // Directional albedo of the lobe for energy preservation and lobe picking
};

float2 GGXEnergyLookup(float Roughness, float NoV)
{
    const float r = Roughness;
    const float c = NoV;
    const float E = 1.0 - saturate(pow(r, c / r) * ((r * c + 0.0266916) / (0.466495 + c)));
    const float Ef = Pow5(1 - c) * pow(2.36651 * pow(c, 4.7703 * r) + 0.0387332, r);
    return float2(E, Ef);
}

float3 AverageFresnel(float3 r, float3 g)
{
    return float3(0.087237, 0.087237, 0.087237) + 0.0230685 * g - 0.0864902 * g * g + 0.0774594 * g * g * g
           + 0.782654 * r - 0.136432 * r * r + 0.278708 * r * r * r
           + 0.19744 * g * r + 0.0360605 * g * g * r - 0.2586 * g * r * r;
}

float GetDiffuseEnergyPreservation(float F)
{
    return 1.f - F;
}
float3 GetDiffuseEnergyPreservation(float3 F)
{
    return 1.f - F;
}

float GetSpecularEnergyPreservation(float3 alebdo, float roughness, float NoL, float NoV)
{
    NoL = saturate(NoL);
    NoV = saturate(NoV);
    
    SamplerState warpLinearSampler = SamplerDescriptorHeap[WarpLinearSampler];
    
    Texture2D<float> GGX_E_LUT = ResourceDescriptorHeap[GGX_E_LUT_Index];
    Texture2D<float> GGX_EAVG_LUT = ResourceDescriptorHeap[GGX_Eavg_LUT_Index];
    
    float3 E_o = GGX_E_LUT.SampleLevel(warpLinearSampler, float2(NoL, roughness), 0);
    float3 E_i = GGX_E_LUT.SampleLevel(warpLinearSampler, float2(NoV, roughness), 0);
    float3 E_avg = GGX_EAVG_LUT.SampleLevel(warpLinearSampler, float2(0, roughness), 0);

    float3 edgetint = float3(0.827, 0.792, 0.678);
    float3 F_avg = AverageFresnel(alebdo, edgetint);
    float3 F_ms = (1.f - E_o) * (1.f - E_i) / (PI * (1.f - E_avg));
    float3 F_add = F_avg * E_avg / (1.f - F_avg * (1.f - E_avg));

    return F_add * F_ms;
}

FBxDFEnergyTerms ComputeFresnelEnergyTerms(float2 E, float3 InF0)
{
    float3 F0 = InF0;
    const float F90 = saturate(50.0 * F0.g);
    bool bShadingEnergyConservation = true;

    FBxDFEnergyTerms Result;
	// [2] Eq 16: this restores the missing energy of the bsdf, while also accounting for the fact that the fresnel term causes some energy to be absorbed
	// NOTE: using F0 here is an approximation, but for schlick fresnel Favg is almost exactly equal to F0
    Result.W = 1.0 + F0 * ((1 - E.x) / E.x);
	// Now estimate the amount of energy reflected off this specular lobe so that we can remove it from underlying BxDF layers (like diffuse)
	// This relies on the split-sum approximation as in [3] Sec 4.
	// This term can also be useful to compute the probability of choosing among lobes
	Result.E = Result.W * (E.x * F0 + E.y * (F90 - F0));
	return Result;
}

// Return the energy absorbed by upper layer (e.g., for the specular layer attenuation onto diffuse)
// Note: Use the directional albedo luminance to avoid color-shift due to metallic specular (for which the energy should be absorbed, not transmitted)
float ComputeEnergyPreservation(FBxDFEnergyTerms EnergyTerms)
{

	return 1 - Luminance(EnergyTerms.E);

}

// Return the energy conservation weight factor for account energy loss in the BSDF model (i.e. due to micro-facet multiple scattering)
float3 ComputeEnergyConservation(FBxDFEnergyTerms EnergyTerms)
{
	return EnergyTerms.W;
}

#endif