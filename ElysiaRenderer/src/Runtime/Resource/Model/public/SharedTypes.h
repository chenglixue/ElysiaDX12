#pragma once


struct MaterialTextureIndices
{
	uint Albedo;
	uint Normal;
	uint Roughness;
	uint Metallic;
	uint Occlusion;
	uint Specular;
	uint Emissive;
	uint Height;

	MaterialTextureIndices Invalid()
	{
		Albedo = UINT_MAX;
		Normal = UINT_MAX;
		Roughness = UINT_MAX;
		Metallic = UINT_MAX;
		Occlusion = UINT_MAX;
		Specular = UINT_MAX;
		Emissive = UINT_MAX;
		Height = UINT_MAX;

		return *this;
	}
};