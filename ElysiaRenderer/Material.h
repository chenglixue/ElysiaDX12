#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;

	struct Material
	{
		Color diffuse;
		Color emission;
		Color specular;
		Color ambient;
		float opacity;
		float shininess;
		float specularIntensity;

		enum { maxTexPath = 128 };
		enum { texCount = 6 };
		char texDiffusePath[maxTexPath];
		char texRoughnessPath[maxTexPath];
		char texMetallicPath[maxTexPath];
		char texEmissionPath[maxTexPath];
		char texNormalPath[maxTexPath];
		char texLightmapPath[maxTexPath];
		char texReflectionPath[maxTexPath]; 

		enum { maxMaterialName = 128 };
		char name[maxMaterialName];

		UINT diffuseTexIndex = 0;
		UINT specularTexIndex = 0;
		UINT emissionTexIndex = 0;
		UINT normalTexIndex = 0;
		UINT lightmapIndex = 0;
		UINT reflectTexIndex = 0;
	};
}